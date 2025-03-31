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

std::string stackSlotToStringLoc(SSACFG const& _cfg, SSACFGEVMCodeTransform::Stack::Slot const& _slot)
{
	return std::visit(util::GenericVisitor{
		[&](SSACFG::ValueId _value) {
			return ssaCfgVarToString(_cfg, _value);
		},
		[](AbstractAssembly::LabelID _label) {
			return "LABEL[" + std::to_string(_label) + "]";
		},
		[](SSACFGFunctionReturnLabel const& _functionReturnLabel)
		{
			yulAssert(_functionReturnLabel.functionCall, "Function return label was null.");
			yulAssert(std::holds_alternative<Identifier>(_functionReturnLabel.functionCall->functionName));
			return fmt::format("ReturnLabel[{}]", std::get<Identifier>(_functionReturnLabel.functionCall->functionName).name.str());
		}
	}, _slot);
}
std::string stackToStringLoc(SSACFG const& _cfg, std::vector<SSACFGEVMCodeTransform::Slot> const& _stack)
{
	return "[" + util::joinHumanReadable(_stack | ranges::views::transform([&](auto const& _slot) { return stackSlotToStringLoc(_cfg, _slot); })) + "]";
}

SSACFG::LiteralValue resolveLiteralValue(SSACFGEVMCodeTransform::Slot const& _slot, SSACFG const& _cfg)
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
	StackWithAssemblyOps(SSACFG const& _cfg, AbstractAssembly& _assembly, DataStack& _stack, std::map<FunctionCall const*, AbstractAssembly::LabelID> const& _returnLabels):
 		m_cfg(_cfg),
		m_assembly(_assembly),
		m_dataStack(_stack),
		m_returnLabels(_returnLabels)
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

	std::optional<size_t> slotDepth(Slot const& _slot) const {
		return m_dataStack.slotDepth(_slot);
	}

	size_t size() const { return m_dataStack.size(); }

	Slot const& operator[](size_t const _index) const { return m_dataStack[_index]; }

	void pushOrDup(Slot const& _slot)
	{
		std::visit(util::GenericVisitor{
			[&](SSACFG::ValueId _value) {
				if (!m_cfg.isLiteralValue(_value))
				{
					auto const depth = slotDepth(_slot);
					yulAssert(depth, fmt::format("Tried bringing up {}", ssaCfgVarToString(m_cfg, _value)));
					m_assembly.appendInstruction(evmasm::dupInstruction(static_cast<unsigned>(*depth + 1)));
					m_dataStack.dup(*depth);
				}
				else
					push(_value);
			},
			[&](AbstractAssembly::LabelID const _label) {
				m_assembly.appendLabelReference(_label);
			},
			[&](SSACFGFunctionReturnLabel const& _label)
			{
				auto const* maybeLabel = util::valueOrNullptr(m_returnLabels, _label.functionCall);
				yulAssert(maybeLabel);
				m_dataStack.push(_label);
				m_assembly.appendLabelReference(*maybeLabel);
			}
		}, _slot);
	}

	auto begin() const { return ranges::begin(m_dataStack); }
	auto end() const { return ranges::end(m_dataStack); }
private:
	SSACFG const& m_cfg;
	AbstractAssembly& m_assembly;
	DataStack& m_dataStack;
	std::map<FunctionCall const*, AbstractAssembly::LabelID> const& m_returnLabels;
};
static_assert(SSACFGStack<StackWithAssemblyOps>);

}

std::vector<StackTooDeepError> SSACFGEVMCodeTransform::run(
	AbstractAssembly& _assembly,
	ControlFlowLiveness const& _liveness,
	BuiltinContext& _builtinContext,
	UseNamedLabels _useNamedLabelsForFunctions)
{
	if constexpr (debugOutput)
	{
		std::cout << "\n\n\n";
		std::cout << "--------------------\n";
		std::cout << "Running SSACFGEVMCodeTransform" << std::endl;
		std::cout << "--------------------\n";
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
	m_stackLayout(SSACFGStackLayoutGenerator::generate(_liveness)),
	m_functionLabels(std::move(_functionLabels)),
	m_generatedBlocks(_cfg.numBlocks(), false)
{
	for (size_t i = 0; i < _cfg.numBlocks(); ++i)
		m_blockLabels.emplace_back(m_assembly.newLabelId());
	//m_blockLabels.resize(_cfg.numBlocks());
	//for (auto const& blockId: m_liveness.topologicalSort().preOrder())
	//	m_blockLabels[blockId] = m_assembly.newLabelId();
}

void SSACFGEVMCodeTransform::transformFunction(Scope::Function const& _function)
{
	auto const label = functionLabel(_function);
	if constexpr (debugOutput)
		std::cout << "Generating code for function " << _function.name.str() << ", label=" << label << std::endl;
	m_assembly.appendLabel(label);
	m_assembly.setStackHeight(static_cast<int>(_function.numArguments));
	m_stack = m_stackLayout.blockLayouts[m_cfg.entry.value].stackIn;
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

	m_assembly.appendLabel(m_blockLabels[_block.value]);
	if constexpr (debugOutput)
		std::cout << "\tGenerating for Block " << _block.value << " with label " << m_blockLabels[_block.value] << std::endl;

	auto const& blockLayout = m_stackLayout[_block];
	yulAssert(blockLayout.stackIn == m_stack, fmt::format("{} =/= {}", stackToStringLoc(m_cfg, blockLayout.stackIn.stackData()), stackToStringLoc(m_cfg, m_stack.stackData())));
	// todo assert on all exits that the stack height is fine
	yulAssert(static_cast<int>(m_stack.size()) == m_assembly.stackHeight());

	yulAssert(m_cfg.block(_block).operations.size() == blockLayout.operationIn.size(), "We need as many stack layouts as we have operations");

	// for each op with respective live-out, descend into op
	for (auto const& [operation, operationStackIn]: ranges::views::zip( m_cfg.block(_block).operations, blockLayout.operationIn))
	{
		bool const hasReturnLabel = std::holds_alternative<SSACFG::Call>(operation.kind)
									&& std::get<SSACFG::Call>(operation.kind).canContinue;
		if (hasReturnLabel)
			m_returnLabels[&std::get<SSACFG::Call>(operation.kind).call.get()] = m_assembly.newLabelId();

		yulAssert(static_cast<int>(m_stack.size()) == m_assembly.stackHeight());
		// Create required layout for entering the operation.
		DanielShuffler<StackWithAssemblyOps>::
			shuffle(StackWithAssemblyOps(m_cfg, m_assembly, m_stack, m_returnLabels), {}, operationStackIn.stackData());

		// Assert that we have the inputs of the operation on stack top.
		yulAssert(static_cast<int>(m_stack.size()) == m_assembly.stackHeight());
		// todo assert that we have return label + inputs on the top of the stack

		yulAssert(m_stack.size() >= operation.inputs.size() + (hasReturnLabel ? 1 : 0));
		for (auto const& [stackEntry, input]: ranges::zip_view(
			m_stack | ranges::views::take_last(operation.inputs.size()),
			operation.inputs
		))
			yulAssert(stackEntry == Stack::Slot{input});
		if (hasReturnLabel)
		{
			auto const returnLabelSlot = *(ranges::rbegin(m_stack) + static_cast<std::ptrdiff_t>(operation.inputs.size()));
			yulAssert(std::holds_alternative<SSACFG::Call>(operation.kind));
			yulAssert(returnLabelSlot == Slot{SSACFGFunctionReturnLabel{&std::get<SSACFG::Call>(operation.kind).call.get()}});
		}
		size_t const baseHeight = m_stack.size() - operation.inputs.size() - (hasReturnLabel ? 1 : 0);

		// Perform the operation.
		performOperation(operation);

		// Assert that the operation produced its proclaimed output.
		// yulAssert(static_cast<int>(m_stack.size()) == m_assembly.stackHeight());
		yulAssert(m_stack.size() == baseHeight + operation.outputs.size());
		for (auto const& [stackEntry, output]: ranges::zip_view(
			m_stack | ranges::views::take_last(operation.outputs.size()),
			operation.outputs
		))
			yulAssert(stackEntry == Stack::Slot{output});
		yulAssert(
			static_cast<int>(m_stack.size()) == m_assembly.stackHeight(),
			fmt::format("symbolic stack size = {} =/= {} = assembly stack height", m_stack.size(), m_assembly.stackHeight())
		);
	}

	util::GenericVisitor exitVisitor{
		[&](SSACFG::BasicBlock::MainExit const& /*_mainExit*/)
		{
			m_assembly.appendInstruction(evmasm::Instruction::STOP);
		},
		[&](SSACFG::BasicBlock::Jump const& _jump)
		{
			if constexpr (debugOutput)
				std::cout << "\t\tJUMP Creating target stack for jump " << _block.value << " -> " << _jump.target.value << std::endl;

			shuffleStack(m_stackLayout[_jump.target].stackIn.stackData(), SSACFG::Edge{_block, _jump.target});
			m_assembly.appendJumpTo(m_blockLabels[_jump.target.value]);
			if (!m_generatedBlocks[_jump.target.value])
				(*this)(_jump.target);
		},
		[&](SSACFG::BasicBlock::ConditionalJump const& _conditionalJump)
		{
			{
				auto stackIn = m_stackLayout[_conditionalJump.nonZero].stackIn.stackData();
				stackIn.emplace_back(_conditionalJump.condition);
				if constexpr (debugOutput)
					std::cout << "\t\tJUMPI Creating stack for nonZero layout " << stackToStringLoc(m_cfg, m_stack.stackData()) << " -> " << stackToStringLoc(m_cfg, stackIn) << std::endl;
				shuffleStack(stackIn, SSACFG::Edge{_block, _conditionalJump.nonZero});
			}
			// std::cout << "Stack after putting cond on top: "<< stackToStringLoc(m_cfg, m_stack.stackData()) << std::endl;

			// Emit the conditional jump to the non-zero label and update the stored stack.
			{
				yulAssert(m_stack.top() == Slot{_conditionalJump.condition});
				m_assembly.appendJumpToIf(m_blockLabels[_conditionalJump.nonZero.value]);
				// update symbolic stack by popping the condition
				m_stack.pop();
			}
			Stack const nonZeroStack = m_stack;

			if constexpr (debugOutput)
				std::cout << "\t\tJUMPI Creating stack for zero layout " << stackToStringLoc(m_cfg, m_stack.stackData()) << " -> " << stackToStringLoc(m_cfg, m_stackLayout[_conditionalJump.zero].stackIn.stackData()) << std::endl;

			shuffleStack(
				m_stackLayout[_conditionalJump.zero].stackIn.stackData(),
				SSACFG::Edge{_block, _conditionalJump.zero}
			);
			m_assembly.appendJumpTo(m_blockLabels[_conditionalJump.zero.value]);

			if (!m_generatedBlocks[_conditionalJump.zero.value])
				(*this)(_conditionalJump.zero);

			m_stack = nonZeroStack;
			m_assembly.setStackHeight(static_cast<int>(m_stack.size()));
			if (!m_generatedBlocks[_conditionalJump.nonZero.value])
				(*this)(_conditionalJump.nonZero);
		},
		[&](SSACFG::BasicBlock::JumpTable const&){ yulAssert(false, "Jump tables not yet implemented."); },
		[&](SSACFG::BasicBlock::FunctionReturn const& _return){
			// Need to be able to also swap up return label!
			yulAssert(static_cast<size_t>(m_assembly.stackHeight()) == m_stack.size());
			m_assembly.setStackHeight(m_assembly.stackHeight()+1);
			std::vector<SSACFGStackLayout::Slot> returnSlots;
			if (!_return.returnValues.empty())
			{
				returnSlots.assign(_return.returnValues.begin()+1, _return.returnValues.end());
				returnSlots.emplace_back(_return.returnValues.front());
				shuffleStack(returnSlots);
				m_assembly.appendInstruction(evmasm::swapInstruction(static_cast<unsigned>(_return.returnValues.size())));
			}
			else
				shuffleStack(returnSlots);
			m_assembly.appendJump(0, AbstractAssembly::JumpType::OutOfFunction);
			// m_assembly.setStackHeight(static_cast<int>(m_stack.size()) + 1);
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
	yulAssert(static_cast<int>(m_stack.size()) == m_assembly.stackHeight());
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
			auto const* returnLabel = util::valueOrNullptr(m_returnLabels, &_call.call.get());
			yulAssert(!!returnLabel == _call.canContinue);
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
			{
				m_assembly.appendLabel(*returnLabel);
				m_stack.pop();
			}
		},
	}, _operation.kind);
	for (size_t i = 0; i < _operation.inputs.size(); ++i)
		m_stack.pop();
	for (auto value: _operation.outputs)
		m_stack.push(value);

	if constexpr (debugOutput)
		std::cout << " -> " << stackToStringLoc(m_cfg, m_stack.stackData()) << std::endl;
}

void SSACFGEVMCodeTransform::shuffleStack(std::vector<Slot> _target, std::optional<SSACFG::Edge> const& _edge)
{
	auto const transform = _edge ? ReversePhiFunctionTransform(m_cfg, _edge->from, _edge->to) : ReversePhiFunctionTransform{};
	auto const transformedTarget = [&]
	{
		if (transform.noOp())
			return _target;
		return _target | ranges::views::transform(transform) | ranges::to<std::vector>;
	}();
	DanielShuffler<StackWithAssemblyOps>::shuffle(
		StackWithAssemblyOps(m_cfg, m_assembly, m_stack, m_returnLabels),
		{}, transformedTarget
	);
	yulAssert(transformedTarget == m_stack.stackData());
	m_stack = Stack(_target);
}
