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
#include <libyul/backends/evm/SSACFGStackLayoutGenerator.h>
#include <libyul/backends/evm/SSACFGStackShuffler.h>

#include <libsolutil/StringUtils.h>
#include <libsolutil/Visitor.h>
#include <range/v3/algorithm/none_of.hpp>

#include <range/v3/range/conversion.hpp>
#include <range/v3/view/drop_exactly.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/view/take_last.hpp>
#include <range/v3/view/zip.hpp>

using namespace solidity;
using namespace solidity::yul;

namespace
{

constexpr bool debugOutput = true;

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

std::string stackSlotToStringLoc(SSACFG const& _cfg, SSACFGEVMCodeTransform::Stack::Slot const& _slot)
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

SSACFG::LiteralValue resolveLiteralValue(ssacfg::StackSlot const& _slot, SSACFG const& _cfg)
{
	yulAssert(std::holds_alternative<SSACFG::ValueId>(_slot));
	auto const& valueId = std::get<SSACFG::ValueId>(_slot);
	return std::visit(util::GenericVisitor{
			[&](SSACFG::LiteralValue const& _literal) {
				return _literal;
			},
			[&](auto const&) -> SSACFG::LiteralValue { solAssert(false, fmt::format("Tried bringing up v{}", valueId.value)); }
	}, _cfg.valueInfo(valueId));
}

class StackWithAssemblyOps
{
public:
	using DataStack = SSACFGEVMCodeTransform::Stack;
	using Slot = DataStack::Slot;
	StackWithAssemblyOps(SSACFG const& _cfg, AbstractAssembly& _assembly, DataStack& _stack):
 		m_cfg(_cfg),
		m_assembly(_assembly),
		m_dataStack(_stack)
	{}

	Slot const& top() const { return m_dataStack.top(); }
	void swap(size_t const _depth)
	{
		m_dataStack.swap(_depth);
		m_assembly.appendInstruction(evmasm::swapInstruction(static_cast<unsigned>(_depth)));
	}
	void pop()
	{
		m_dataStack.pop();
		m_assembly.appendInstruction(evmasm::Instruction::POP);
	}
	void push(Slot const& _slot)
	{
		m_dataStack.push(_slot);
		m_assembly.appendConstant(resolveLiteralValue(_slot, m_cfg).value);
	}

	std::optional<size_t> slotIndex(Slot const& _slot) const {
		return m_dataStack.slotIndex(_slot);
	}

	size_t size() const { return m_dataStack.size(); }

	Slot const& operator[](size_t const _depth) const { return m_dataStack[_depth]; }

	void pushAll(StackWithAssemblyOps const& _other) {
		yulAssert(&_other.m_assembly == &m_assembly);
		m_dataStack.pushAll(_other.m_dataStack);
	}

	void pushOrDup(Slot const& _slot)
	{
		std::visit(util::GenericVisitor{
			[&](SSACFG::ValueId _value) {
				if (!m_cfg.isLiteralValue(_value))
				{
					auto const slotIndexValue = slotIndex(_slot);
					yulAssert(slotIndexValue, fmt::format("Tried bringing up {}", ssaCfgVarToString(m_cfg, _value)));
					m_assembly.appendInstruction(evmasm::dupInstruction(static_cast<unsigned>(*slotIndexValue + 1)));
				}
				else
					push(_value);
			},
			[&](AbstractAssembly::LabelID const _label) {
				m_assembly.appendLabelReference(_label);
			}
		}, _slot);
		m_dataStack.push(_slot);
	}

	auto begin() const { return ranges::begin(m_dataStack); }
	auto end() const { return ranges::end(m_dataStack); }
private:
	SSACFG const& m_cfg;
	AbstractAssembly& m_assembly;
	DataStack& m_dataStack;
};
static_assert(SSACFGStack<StackWithAssemblyOps>);

void shuffleStack(
	SSACFG const& _cfg,
	AbstractAssembly& _assembly,
	SSACFGEVMCodeTransform::Stack& _stack,
	std::vector<SSACFGEVMCodeTransform::Stack::Slot> _target,
	ReversePhiFunctionTransform const& _phiTransform = {}
)
{
	auto const transformedTarget = [&]
	{
		if (_phiTransform.noOp())
			return _target;
		return _target | ranges::views::transform(_phiTransform) | ranges::to<std::vector>;
	}();
	DanielShuffler<StackWithAssemblyOps>::shuffle(
		StackWithAssemblyOps(_cfg, _assembly, _stack),
		transformedTarget
	);
	yulAssert(transformedTarget == _stack.stackData());
	_stack = SSACFGEVMCodeTransform::Stack(transformedTarget);
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
	mainCodeTransform(controlFlow.mainGraph->entry);

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
	m_stackLayout(SSACFGStackLayoutGenerator::generate(_liveness)),
	m_functionLabels(std::move(_functionLabels)),
	m_generatedBlocks(_cfg.numBlocks(), false),
	m_blockLabels(_cfg.numBlocks(), std::nullopt)
{ }

void SSACFGEVMCodeTransform::transformFunction(Scope::Function const& _function)
{
	// Force function entry block to start from initial function layout.
	auto const label = functionLabel(_function);
	if constexpr (debugOutput)
		std::cout << "Generating code for function " << _function.name.str() << ", label=" << label << std::endl;
	m_assembly.appendLabel(label);
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
	yulAssert(!m_generatedBlocks[_block.value], "Each block is transformed exactly once.");
	m_generatedBlocks[_block.value] = true;

	ScopedSaveAndRestore stackSave{m_stack, Stack{}};

	{
		auto& maybeBlockLabel = m_blockLabels[_block.value];
		if (!maybeBlockLabel)
			maybeBlockLabel = m_assembly.newLabelId();
		m_assembly.appendLabel(*maybeBlockLabel);

		if constexpr (debugOutput)
			std::cout << "\tGenerating for Block " << _block.value << " with label " << *maybeBlockLabel << std::endl;
	}

	auto const& blockLayout = m_stackLayout[_block];
	yulAssert(blockLayout.stackIn == m_stack);
	yulAssert(static_cast<int>(m_stack.size()) == m_assembly.stackHeight());

	m_assembly.setStackHeight(static_cast<int>(m_stack.size()));
	// check that we have as much liveness info as there are ops in the block
	yulAssert(m_cfg.block(_block).operations.size() == m_liveness.operationsLiveOut(_block).size());

	// for each op with respective live-out, descend into op
	for (auto const& [operation, operationStackIn]: ranges::views::zip( m_cfg.block(_block).operations, blockLayout.operationIn))
	{
		// Create required layout for entering the operation.
		DanielShuffler<StackWithAssemblyOps>::shuffle(
			StackWithAssemblyOps(m_cfg, m_assembly, m_stack),
			operationStackIn.stackData()
		);

		// Assert that we have the inputs of the operation on stack top.
		//yulAssert(static_cast<int>(m_stack.size()) == m_assembly.stackHeight());
		yulAssert(m_stack.size() >= operation.inputs.size());
		size_t baseHeight = m_stack.size() - operation.inputs.size();
		for (auto const& [stackEntry, input]: ranges::zip_view(
			m_stack | ranges::views::take_last(operation.inputs.size()),
			operation.inputs
		))
			yulAssert(stackEntry == Stack::Slot{input});

		// Perform the operation.
		performOperation(operation);

		// Assert that the operation produced its proclaimed output.
		// yulAssert(static_cast<int>(m_stack.size()) == m_assembly.stackHeight());
		yulAssert(m_stack.size() == baseHeight + operation.outputs.size());
		yulAssert(m_stack.size() >= operation.outputs.size());
		for (auto const& [stackEntry, output]: ranges::zip_view(
			m_stack | ranges::views::take_last(operation.outputs.size()),
			operation.outputs
		))
			yulAssert(stackEntry == Stack::Slot{output});
	}

	util::GenericVisitor exitVisitor{
		[&](SSACFG::BasicBlock::MainExit const& /*_mainExit*/)
		{
			m_assembly.appendInstruction(evmasm::Instruction::STOP);
		},
		[&](SSACFG::BasicBlock::Jump const& _jump)
		{
			if (!m_blockLabels[_jump.target.value])
				m_blockLabels[_jump.target.value] = m_assembly.newLabelId();

			if constexpr (debugOutput)
				std::cout << "\t\tJUMP Creating target stack for jump " << _block.value << " -> " << _jump.target.value << std::endl;

			ReversePhiFunctionTransform const phiTransform(m_cfg, _block, _jump.target);
			shuffleStack(m_cfg, m_assembly, m_stack, m_stackLayout[_jump.target].stackIn.stackData(), phiTransform);
			m_assembly.appendJumpTo(*m_blockLabels[_jump.target.value]);
			if (!m_generatedBlocks[_jump.target.value])
				(*this)(_jump.target);
		},
		[&](SSACFG::BasicBlock::ConditionalJump const& _conditionalJump)
		{
			{
				ReversePhiFunctionTransform const phiTransform(m_cfg, _block, _conditionalJump.nonZero);
				shuffleStack(m_cfg, m_assembly, m_stack, m_stackLayout[_conditionalJump.nonZero].stackIn.stackData(), phiTransform);
			}
			yulAssert(m_stack.top() == ssacfg::StackSlot{_conditionalJump.condition});

			// Emit the conditional jump to the non-zero label and update the stored stack.
			auto& nonZeroLabel = m_blockLabels[_conditionalJump.nonZero.value];
			if (!nonZeroLabel)
				nonZeroLabel = m_assembly.newLabelId();
			m_assembly.appendJumpToIf(*nonZeroLabel);
			m_stack.pop();

			if constexpr (debugOutput)
				std::cout << "\t\tJUMPI Creating stack for zero layout" << std::endl;

			{
				ReversePhiFunctionTransform const phiTransform(m_cfg, _block, _conditionalJump.zero);
				shuffleStack(m_cfg, m_assembly, m_stack, m_stackLayout[_conditionalJump.zero].stackIn.stackData(), phiTransform);
			}

			auto& zeroLabel = m_blockLabels[_conditionalJump.zero.value];
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
			std::vector<SSACFGStackLayout::Slot> const returnSlots (_return.returnValues.begin(), _return.returnValues.end());
			// m_assembly.setStackHeight(static_cast<int>(m_stack.size()) + 1);
			shuffleStack(m_cfg, m_assembly, m_stack, returnSlots);
			m_assembly.appendJump(0, AbstractAssembly::JumpType::OutOfFunction);
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

void SSACFGEVMCodeTransform::performOperation(SSACFG::Operation const& _operation)
{
	std::optional<AbstractAssembly::LabelID> returnLabel;
	if (auto const* call = std::get_if<SSACFG::Call>(&_operation.kind))
		if (call->canContinue)
		{
			returnLabel = m_assembly.newLabelId();
		}
	std::visit(util::GenericVisitor {
		[&](SSACFG::BuiltinCall const& _builtin) {
			if constexpr (debugOutput)
				std::cout << "\t\t\tBuiltin call: " << _builtin.builtin.get().name << ": " << stackToStringLoc(m_cfg, m_stack.stackData());
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
				std::cout << "\t\t\tCall: " << _call.function.get().name.str() << " (label=" << functionLabel(_call.function) << ")" << ": " << stackToStringLoc(m_cfg, m_stack.stackData());
				if (returnLabel)
					std::cout << ", returnLabel: " << *returnLabel;
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
		m_stack.pop();
	for (auto value: _operation.outputs)
		m_stack.push(value);

	if constexpr (debugOutput)
		std::cout << " -> " << stackToStringLoc(m_cfg, m_stack.stackData()) << std::endl;
}
