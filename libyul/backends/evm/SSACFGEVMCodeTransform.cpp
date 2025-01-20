/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0


#include <libyul/backends/evm/SSACFGEVMCodeTransform.h>

#include <libyul/backends/evm/ControlFlowGraph.h>
#include <libyul/backends/evm/SSAControlFlowGraphBuilder.h>
#include <libyul/backends/evm/StackHelpers.h>

#include <libsolutil/StringUtils.h>
#include <libsolutil/Visitor.h>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop_exactly.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/zip.hpp>

using namespace solidity;
using namespace solidity::yul;

namespace
{

constexpr bool debugOutput = false;

std::string ssaCfgVarToString(SSACFG const& _cfg, SSACFG::ValueId _var)
{
	if (_var.value == std::numeric_limits<size_t>::max())
		return "INVALID";
	auto const& info = _cfg.valueInfo(_var);
	return std::visit(
		util::GenericVisitor{
			[&](SSACFG::UnreachableValue const&) -> std::string {
				return "[unreachable]";
			},
			[&](SSACFG::LiteralValue const& _literal) {
				std::stringstream str;
				str << _literal.value;
				return str.str();
			},
			[&](auto const&) {
				return "v" + std::to_string(_var.value);
			}
		},
		info
	);
}

std::string stackSlotToStringLoc(SSACFG const& _cfg, ssacfg::StackSlot const& _slot)
{
	return std::visit(util::GenericVisitor{
		[&](SSACFG::ValueId _value) {
			return ssaCfgVarToString(_cfg, _value);
		},
		[](AbstractAssembly::LabelID _label) {
			return "LABEL[" + std::to_string(_label) + "]";
		}
	}, _slot);
}
std::string stackToStringLoc(SSACFG const& _cfg, std::vector<ssacfg::StackSlot> const& _stack)
{
	return "[" + util::joinHumanReadable(_stack | ranges::views::transform([&](ssacfg::StackSlot const& _slot) { return stackSlotToStringLoc(_cfg, _slot); })) + "]";
}
}

std::vector<ssacfg::StackSlot>
ssacfg::PhiMapping::transformStackToPhiValues(std::vector<StackSlot> const& _stack) const
{
	return _stack | ranges::views::transform([this](StackSlot const& _slot) { return transform(_slot); }) | ranges::to<std::vector>;
}

ssacfg::StackSlot ssacfg::PhiMapping::transform(StackSlot const& _slot) const
{
	if (auto* valueId = std::get_if<SSACFG::ValueId>(&_slot))
	{
		// todo unrecurse, protect against cycles
		auto const it = m_reverseMapping.find(*valueId);
		if (it == m_reverseMapping.end())
			return _slot;
		return it->second;
	}
	return _slot;
}

void ssacfg::Stack::pop(bool _generateInstruction)
{
	yulAssert(!m_stack.empty());
	m_stack.pop_back();
	if (_generateInstruction)
		m_assembly.get().appendInstruction(evmasm::Instruction::POP);
}

void ssacfg::Stack::swap(size_t const _depth, bool _generateInstruction)
{
	yulAssert(m_stack.size() > _depth);
	std::swap(m_stack[m_stack.size() - _depth - 1], m_stack.back());
	if (_generateInstruction)
		m_assembly.get().appendInstruction(evmasm::swapInstruction(static_cast<unsigned>(_depth)));
}

SSACFG::LiteralValue ssacfg::Stack::resolveLiteralValue(StackSlot const& _slot) const
{
	yulAssert(std::holds_alternative<SSACFG::ValueId>(_slot));
	auto const& valueId = std::get<SSACFG::ValueId>(_slot);
	return std::visit(util::GenericVisitor{
			[&](SSACFG::LiteralValue const& _literal) {
				return _literal;
			},
			[&](auto const&) -> SSACFG::LiteralValue { solAssert(false, fmt::format("Tried bringing up v{}", valueId.value)); }
	}, m_cfg.get().valueInfo(valueId));
}

void ssacfg::Stack::push(SSACFG::ValueId const& _value, bool _generateInstruction)
{
	m_stack.emplace_back(_value);
	if (_generateInstruction)
		m_assembly.get().appendConstant(resolveLiteralValue(_value).value);
}

void ssacfg::Stack::dup(size_t const _depth, bool _generateInstruction)
{
	m_stack.push_back(m_stack[m_stack.size() - _depth - 1]);
	if (_generateInstruction)
		m_assembly.get().appendInstruction(evmasm::dupInstruction(static_cast<unsigned>(_depth + 1)));
}

bool ssacfg::Stack::dup(StackSlot const& _slot, bool _generateInstruction)
{
	auto offset = slotIndex(_slot);
	if (offset)
		dup(*offset, _generateInstruction);
	return offset.has_value();
}

std::optional<size_t> ssacfg::Stack::slotIndex(StackSlot const& _slot) const
{
	auto const offset = util::findOffset(m_stack | ranges::views::reverse, _slot);
	if (offset)
		yulAssert(m_stack[m_stack.size() - *offset - 1] == _slot);
	return offset;
}


void ssacfg::Stack::bringUpSlot(StackSlot const& _slot)
{
	std::visit(util::GenericVisitor{
		[&](SSACFG::ValueId _value) {
			if (!dup(_slot))
				push(_value);
		},
		[&](AbstractAssembly::LabelID _label) {
			m_assembly.get().appendLabelReference(_label);
			m_stack.emplace_back(_label);
		}
	}, _slot);
}

void ssacfg::Stack::createExactStack(std::vector<StackSlot> const& _target, PhiMapping const& _phis)
{
	struct ShuffleOperations
	{
		Stack& currentStack;
		std::map<StackSlot, size_t> sourceCounts;
		std::vector<StackSlot> const& targetStack;
		std::map<StackSlot, size_t> targetCounts;

		ShuffleOperations(
			ssacfg::Stack& _currentStack,
			std::vector<StackSlot> const& _targetStack
		): currentStack(_currentStack), targetStack(_targetStack)
		{
			auto const histogram = [](std::vector<StackSlot> const& _stack)
			{
				std::map<StackSlot, size_t> counts;
				for (auto const& targetSlot: _stack)
					counts[targetSlot]++;
				return counts;
			};
			targetCounts = histogram(targetStack);
			sourceCounts = histogram(currentStack.data());
		}

		bool isCompatible(size_t _source, size_t _target) const
		{
			return _source < currentStack.size() && _target < targetStack.size() && currentStack.data()[_source] == targetStack[_target];
		}

		bool sourceIsSame(size_t _sourceOffset1, size_t _sourceOffset2) const
		{
			return _sourceOffset1 < currentStack.size() && _sourceOffset2 < currentStack.size() && currentStack.data()[_sourceOffset1] == currentStack.data()[_sourceOffset2];
		}

		int sourceMultiplicity(size_t _sourceOffset) const
		{
			auto const& slot = currentStack.data()[_sourceOffset];
			return static_cast<int>(util::valueOrDefault(targetCounts, slot, static_cast<size_t>(0))) - static_cast<int>(sourceCounts.at(slot));
		}

		int targetMultiplicity(size_t _targetOffset) const
		{
			auto const& slot = targetStack[_targetOffset];
			return static_cast<int>(targetCounts.at(slot)) - static_cast<int>(util::valueOrDefault(sourceCounts, slot, static_cast<size_t>(0)));
		}

		bool targetIsArbitrary(size_t) const { return false; }

		size_t sourceSize() const { return currentStack.size(); }
		size_t targetSize() const { return targetStack.size(); }

		void swap(size_t _depth)
		{
			currentStack.swap(_depth);
		}

		void pop()
		{
			currentStack.pop();
		}

		void pushOrDupTarget(size_t _targetOffset)
		{
			currentStack.bringUpSlot(targetStack[_targetOffset]);
		}

	};
	if (_phis.empty())
	{
		Shuffler<ShuffleOperations>::shuffle(*this, _target);
		return;
	}

	auto const mappedTarget = _phis.transformStackToPhiValues(_target);
	Shuffler<ShuffleOperations>::shuffle(*this, mappedTarget);
	m_stack = _target;
}
void ssacfg::Stack::createStack(
	std::vector<StackSlot> const& _top,
	std::vector<StackSlot> const& _rest,
	PhiMapping const& _phis
)
{
	createExactStack(_rest + _top, _phis);
}

bool ssacfg::Stack::empty() const
{
	return m_stack.empty();
}

void ssacfg::Stack::clear()
{
	m_stack.clear();
}


void ssacfg::Stack::permute(std::vector<StackSlot> const& _target)
{
	if constexpr (debugOutput)
		std::cout << fmt::format("\t\tPermuting to exact stack {} -> {}", stackToStringLoc(m_cfg.get(), m_stack), stackToStringLoc(m_cfg.get(), _target)) << std::endl;

	{
		auto const histogram = [](std::vector<StackSlot> const& _stack)
		{
			std::map<StackSlot, size_t> counts;
			for (auto const& targetSlot: _stack)
				counts[targetSlot]++;
			return counts;
		};
		auto const targetCounts = histogram(_target);
		auto const stackCounts = histogram(m_stack);
		// first, remove everything from the stack that occurs more often than what's in the target
		for (auto const& [slot, count]: stackCounts)
		{
			size_t targetCount = 0;
			if (auto it = targetCounts.find(slot); it != targetCounts.end())
				targetCount = it->second;
			if (count > targetCount)
				for (size_t i = 0; i < count - targetCount; ++i)
				{
					auto depth = util::findOffset(m_stack | ranges::views::reverse, slot);
					yulAssert(depth);
					if (depth > 0)
						swap(*depth);
					pop();
				}
		}
		// then dup/push stuff that's not there yet in appropriate quantities
		for (auto const& [slot, targetCount]: targetCounts)
		{
			auto findIt = stackCounts.find(slot);
			if (findIt == stackCounts.end())
				for (size_t i = 0; i < targetCount; ++i)
					bringUpSlot(slot);
			else
			{
				auto currentCount = std::min(targetCount, findIt->second);
				yulAssert(currentCount <= targetCount);
				for (size_t i = 0; i < targetCount - currentCount; ++i)
				{
					auto const depth = slotIndex(slot);
					yulAssert(depth);
					dup(*depth);
				}
			}
		}
	}
	// now we have the same elements in the stack just in a different order
	yulAssert(size() == _target.size());
	for (size_t i = 0; i < _target.size(); ++i)
	{
		// look at the bottom element of the stack and swap something there if it's not already the correct slot
		if (m_stack[i] != _target[i])
		{
			auto const depth = util::findOffset(m_stack | ranges::views::reverse, _target[i]);
			if (depth > 0)
				swap(*depth);
			yulAssert(top() == _target[i]);
			if (m_stack.size() - 1 - i > 0)
				swap(m_stack.size() - 1 - i);
		}
		yulAssert(
			m_stack[i] == _target[i],
			fmt::format("Stack target mismatch: current[{}] = {} =/= {} = target[{}]", i, stackSlotToStringLoc(m_cfg.get(), m_stack[i]), stackSlotToStringLoc(m_cfg.get(), _target[i]), i)
		);
	}

	yulAssert(size() == _target.size());
	yulAssert(m_stack == _target, fmt::format("Stack target mismatch: current = {} =/= {} = target", stackToStringLoc(m_cfg.get(), m_stack), stackToStringLoc(m_cfg.get(), _target)));
}

std::vector<StackTooDeepError> SSACFGEVMCodeTransform::run(
	AbstractAssembly& _assembly,
	ControlFlowLiveness const& _liveness,
	BuiltinContext& _builtinContext,
	UseNamedLabels _useNamedLabelsForFunctions)
{
	if constexpr (debugOutput)
	{
		std::cout << "Running SSACFGEVMCodeTransform" << std::endl;
		fmt::print("{}\n", _liveness.toDot());
		std::fflush(nullptr);
	}
	auto const& controlFlow = _liveness.controlFlow.get();
	auto functionLabels = registerFunctionLabels(_assembly, controlFlow, _useNamedLabelsForFunctions);

	SSACFGEVMCodeTransform mainCodeTransform(
		_assembly,
		_builtinContext,
		functionLabels,
		*controlFlow.mainGraph,
		*_liveness.mainLiveness
	);

	// Force main entry block to start from an empty stack.
	mainCodeTransform.blockData(SSACFG::BlockId{0}).stackIn = std::make_optional<std::vector<ssacfg::StackSlot>>();
	mainCodeTransform(SSACFG::BlockId{0});

	std::vector<StackTooDeepError> stackErrors;
	if (!mainCodeTransform.m_stackErrors.empty())
		stackErrors = std::move(mainCodeTransform.m_stackErrors);

	yulAssert(controlFlow.functionGraphMapping.size() == _liveness.functionLiveness.size());
	for (size_t functionIndex = 0; functionIndex < controlFlow.functionGraphMapping.size(); ++functionIndex)
	{
		auto const& functionAndGraph = controlFlow.functionGraphMapping[functionIndex];
		auto const& functionLiveness = _liveness.functionLiveness[functionIndex];
		auto const& [function, functionGraph] = functionAndGraph;
		SSACFGEVMCodeTransform functionCodeTransform(
			_assembly,
			_builtinContext,
			functionLabels,
			*functionGraph,
			*functionLiveness
		);
		functionCodeTransform.transformFunction(*function);
		if (!functionCodeTransform.m_stackErrors.empty())
			stackErrors.insert(stackErrors.end(), functionCodeTransform.m_stackErrors.begin(), functionCodeTransform.m_stackErrors.end());
	}
	return stackErrors;
}

SSACFGEVMCodeTransform::FunctionLabels SSACFGEVMCodeTransform::registerFunctionLabels
(
	AbstractAssembly& _assembly,
	ControlFlow const& _controlFlow,
	UseNamedLabels _useNamedLabelsForFunctions
)
{
	FunctionLabels functionLabels;

	for (auto const& [_function, _functionGraph]: _controlFlow.functionGraphMapping)
	{
		std::set<YulString> assignedFunctionNames;
		bool nameAlreadySeen = !assignedFunctionNames.insert(_function->name).second;
		if (_useNamedLabelsForFunctions == UseNamedLabels::YesAndForceUnique)
			yulAssert(!nameAlreadySeen);
		bool useNamedLabel = _useNamedLabelsForFunctions != UseNamedLabels::Never && !nameAlreadySeen;
		functionLabels[_function] = useNamedLabel ?
			_assembly.namedLabel(
				_function->name.str(),
				_functionGraph->arguments.size(),
				_functionGraph->returns.size(),
				_functionGraph->debugData ? _functionGraph->debugData->astID : std::nullopt
			) :
			_assembly.newLabelId();
	}
	return functionLabels;
}

SSACFGEVMCodeTransform::SSACFGEVMCodeTransform
(
	AbstractAssembly& _assembly,
	BuiltinContext& _builtinContext,
	FunctionLabels _functionLabels,
	SSACFG const& _cfg,
	SSACFGLiveness const& _liveness
):
	m_assembly(_assembly),
	m_builtinContext(_builtinContext),
	m_cfg(_cfg),
	m_liveness(_liveness),
	m_functionLabels(std::move(_functionLabels)),
	m_stack(_assembly, _cfg),
	m_blockData(_cfg.numBlocks()),
	m_generatedBlocks(_cfg.numBlocks(), false)
{ }

void SSACFGEVMCodeTransform::transformFunction(Scope::Function const& _function)
{
	// Force function entry block to start from initial function layout.
	auto const label = functionLabel(_function);
	if constexpr (debugOutput)
		std::cout << "Generating code for function " << _function.name.str() << ", label=" << label << std::endl;
	m_assembly.appendLabel(label);
	blockData(m_cfg.entry).stackIn = m_cfg.arguments | ranges::views::reverse | ranges::views::transform([](auto&& _tuple) { return std::get<1>(_tuple); }) | ranges::to<std::vector<ssacfg::StackSlot>>;
	(*this)(m_cfg.entry);
}

bool SSACFGEVMCodeTransform::requiresCleanStack(SSACFG::BlockId const _block) const
{
	util::GenericVisitor constexpr exitVisitor{
		[&](SSACFG::BasicBlock::MainExit const&) { return false; },
		[&](SSACFG::BasicBlock::Terminated const&){ return false; },
		[](auto const&) { return true; }
	};
	return std::visit(exitVisitor, m_cfg.block(_block).exit);
}

void SSACFGEVMCodeTransform::operator()(SSACFG::BlockId const _block)
{
	yulAssert(!m_generatedBlocks[_block.value]);
	m_generatedBlocks[_block.value] = true;

	ScopedSaveAndRestore stackSave{m_stack, ssacfg::Stack{m_assembly, m_cfg}};

	auto& data = blockData(_block);
	if (!data.label)
		data.label = m_assembly.newLabelId();
	m_assembly.appendLabel(*data.label);

	if constexpr (debugOutput)
		std::cout << "\tGenerating for Block " << _block.value << " with label " << data.label.value() << std::endl;

	{
		// copy stackIn into stack
		yulAssert(data.stackIn, fmt::format("No starting layout for block id {}", _block.value));
		m_stack = ssacfg::Stack{m_assembly, m_cfg, *data.stackIn};
	}

	m_assembly.setStackHeight(static_cast<int>(m_stack.size()));
	// check that we have as much liveness info as there are ops in the block
	yulAssert(m_cfg.block(_block).operations.size() == m_liveness.operationsLiveOut(_block).size());

	// for each op with respective live-out, descend into op
	for (auto&& [op, liveOut]: ranges::zip_view(m_cfg.block(_block).operations, m_liveness.operationsLiveOut(_block)))
		(*this)(op, liveOut);

	util::GenericVisitor exitVisitor{
		[&](SSACFG::BasicBlock::MainExit const& /*_mainExit*/)
		{
			m_assembly.appendInstruction(evmasm::Instruction::STOP);
		},
		[&](SSACFG::BasicBlock::Jump const& _jump)
		{
			auto& targetLabel = blockData(_jump.target).label;
			if (!targetLabel)
				targetLabel = m_assembly.newLabelId();
			auto& targetStack = blockData(_jump.target).stackIn;
			if (!targetStack)
				// initial stack layout is just the live-ins (would also suffice to be the stack top)
				targetStack = m_liveness.liveIn(_jump.target) | ranges::to<std::vector<ssacfg::StackSlot>>;

			if constexpr (debugOutput)
				std::cout << "\t\tJUMP Creating target stack for jump " << _block.value << " -> " << _jump.target.value << std::endl;

			m_stack.createExactStack(*targetStack, ssacfg::PhiMapping{m_cfg, _block, _jump.target});
			// todo this requires better stack layout generation
			//if (requiresCleanStack(_jump.target))
			//	m_stack.createExactStack(*targetStack, ssacfg::PhiMapping{m_cfg, _block, _jump.target});
			//else
			//	m_stack.createExactStack(m_stack.data() + *targetStack, ssacfg::PhiMapping{m_cfg, _block, _jump.target});
			m_assembly.appendJumpTo(*targetLabel);
			if (!m_generatedBlocks[_jump.target.value])
				(*this)(_jump.target);
		},
		[&](SSACFG::BasicBlock::ConditionalJump const& _conditionalJump)
		{
			auto& nonZeroLayout = blockData(_conditionalJump.nonZero).stackIn;
			if (!nonZeroLayout)
			{
				auto const liveIn = m_liveness.liveIn(_conditionalJump.nonZero) | ranges::to<std::vector<ssacfg::StackSlot>>;
				ssacfg::PhiMapping zeroBranchMapping {m_cfg, _block, _conditionalJump.zero};
				auto const liveOut = zeroBranchMapping.transformStackToPhiValues(m_liveness.liveIn(_conditionalJump.zero) | ranges::to<std::vector<ssacfg::StackSlot>>);
				nonZeroLayout = (liveOut + liveIn) | ranges::to<std::set> | ranges::to<std::vector>;
				// todo this requires better stack layout generation
				/*if (requiresCleanStack(_conditionalJump.nonZero))
					nonZeroLayout = (liveOut + liveIn) | ranges::to<std::set> | ranges::to<std::vector>;
				else
					nonZeroLayout = m_stack.data() + liveIn;*/
				if constexpr (debugOutput)
					std::cout << "\t\tJUMPI Creating stack for non zero layout" << std::endl;
			}
			m_stack.createExactStack(*nonZeroLayout + std::vector<ssacfg::StackSlot>{_conditionalJump.condition}, ssacfg::PhiMapping{m_cfg, _block, _conditionalJump.nonZero});
			yulAssert(m_stack.top() == ssacfg::StackSlot{_conditionalJump.condition});

			// Emit the conditional jump to the non-zero label and update the stored stack.
			auto& nonZeroLabel = blockData(_conditionalJump.nonZero).label;
			if (!nonZeroLabel)
				nonZeroLabel = m_assembly.newLabelId();
			m_assembly.appendJumpToIf(*nonZeroLabel);
			m_stack.pop(false);

			if constexpr (debugOutput)
				std::cout << "\t\tJUMPI Creating stack for zero layout" << std::endl;
			auto& zeroLayout = blockData(_conditionalJump.zero).stackIn;
			if (!zeroLayout)
				zeroLayout = m_liveness.liveIn(_conditionalJump.zero) | ranges::to<std::vector<ssacfg::StackSlot>>;
			if (requiresCleanStack(_conditionalJump.zero))
				m_stack.createStack(*zeroLayout, {}, ssacfg::PhiMapping{m_cfg, _block, _conditionalJump.zero});
			else
				m_stack.createExactStack(m_stack.data() + *zeroLayout, ssacfg::PhiMapping{m_cfg, _block, _conditionalJump.zero});
			auto& zeroLabel = blockData(_conditionalJump.zero).label;
			if (!zeroLabel)
				zeroLabel = m_assembly.newLabelId();
			m_assembly.appendJumpTo(*zeroLabel);

			if (!m_generatedBlocks[_conditionalJump.zero.value])
				(*this)(_conditionalJump.zero);
			if (!m_generatedBlocks[_conditionalJump.nonZero.value])
				(*this)(_conditionalJump.nonZero);
		},
		[&](SSACFG::BasicBlock::JumpTable const&){ yulAssert(false, "Jump tables not yet implemented."); },
		[&](SSACFG::BasicBlock::FunctionReturn const& _return){
			// Need to be able to also swap up return label!
			yulAssert(static_cast<size_t>(m_assembly.stackHeight()) == m_stack.size());
			m_assembly.setStackHeight(static_cast<int>(m_stack.size()) + 1);
			if (_return.returnValues.empty())
				while (!m_stack.empty())
					m_stack.pop();
			else
			{
				auto targetStack = _return.returnValues | ranges::views::drop_exactly(1) | ranges::to<std::vector<ssacfg::StackSlot>>;
				targetStack.emplace_back(_return.returnValues.front());
				m_stack.createExactStack(targetStack, {});
				// Swap up return label.
				m_assembly.appendInstruction(evmasm::swapInstruction(static_cast<unsigned>(targetStack.size())));
			}
			m_assembly.appendJump(0, AbstractAssembly::JumpType::OutOfFunction);
			m_stack.clear();
		},
		[&](SSACFG::BasicBlock::Terminated const&){
			// TODO: assert that last instruction terminated.
			// To be sure just emit another INVALID - should be removed by optimizer.
			m_assembly.appendInstruction(evmasm::Instruction::INVALID);
		},
		[](auto const&)
		{
			yulAssert(false, "unhandled case");
		}
	};
	std::visit(exitVisitor, m_cfg.block(_block).exit);
}

void SSACFGEVMCodeTransform::operator()(SSACFG::Operation const& _operation, std::set<SSACFG::ValueId> const& _liveOut)
{
	std::vector<ssacfg::StackSlot> requiredStackTop;
	std::optional<AbstractAssembly::LabelID> returnLabel;
	if (auto const* call = std::get_if<SSACFG::Call>(&_operation.kind))
		if (call->canContinue)
		{
			returnLabel = m_assembly.newLabelId();
			requiredStackTop.emplace_back(*returnLabel);
		}
	requiredStackTop += _operation.inputs;
	// literals should have been pulled out a priori and now are treated as push constants
	yulAssert(std::none_of(
		_liveOut.begin(),
		_liveOut.end(),
		[this](SSACFG::ValueId _valueId){ return m_cfg.isLiteralValue(_valueId); }
	));
	auto liveOutWithoutOutputs = std::set<ssacfg::StackSlot>(_liveOut.begin(), _liveOut.end()) - _operation.outputs;
	// todo exploit commutativity
	m_stack.createStack(requiredStackTop, liveOutWithoutOutputs | ranges::to<std::vector>);
	std::visit(util::GenericVisitor {
		[&](SSACFG::BuiltinCall const& _builtin) {
			if constexpr (debugOutput)
				std::cout << "\t\t\tBuiltin call: " << _builtin.builtin.get().name << ": " << stackToStringLoc(m_cfg, m_stack.data()) << std::endl;
			m_assembly.setSourceLocation(originLocationOf(_builtin));
			static_cast<BuiltinFunctionForEVM const&>(_builtin.builtin.get()).generateCode(
				_builtin.call,
				m_assembly,
				m_builtinContext
			);
		},
		[&](SSACFG::Call const& _call) {
			if constexpr (debugOutput)
			{
				std::cout << "\t\t\tCall: " << _call.function.get().name.str() << " (label=" << functionLabel(_call.function) << ")" << ": " << stackToStringLoc(m_cfg, m_stack.data());
				if (returnLabel)
					std::cout << ", returnLabel: " << *returnLabel;
				std::cout << std::endl;
			}
			m_assembly.setSourceLocation(originLocationOf(_call));
			m_assembly.appendJumpTo(
				functionLabel(_call.function),
				static_cast<int>(_call.function.get().numReturns - _call.function.get().numArguments) - (_call.canContinue ? 1 : 0),
				AbstractAssembly::JumpType::IntoFunction
			);
			if (returnLabel)
				m_assembly.appendLabel(*returnLabel);
		},
	}, _operation.kind);
	for (size_t i = 0; i < _operation.inputs.size() + (returnLabel ? 1 : 0); ++i)
		m_stack.pop(false);
	for (auto value: _operation.outputs)
		m_stack.push(value, false);
}
