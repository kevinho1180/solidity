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

#pragma once


#include <libyul/backends/evm/EVMDialect.h>
#include <libyul/backends/evm/ControlFlow.h>
#include <libyul/backends/evm/SSACFGLiveness.h>
#include <libyul/backends/evm/SSAControlFlowGraph.h>
#include <libyul/AST.h>
#include <libyul/Exceptions.h>
#include <libyul/Scope.h>

#include <vector>

namespace solidity::langutil
{
class ErrorReporter;
}

namespace solidity::yul
{

struct AsmAnalysisInfo;
struct StackLayout;

namespace ssacfg
{
using StackSlot = std::variant<SSACFG::ValueId, AbstractAssembly::LabelID>;
class PhiMapping
{
public:
	PhiMapping() = default;
	PhiMapping(SSACFG const& _cfg, SSACFG::BlockId _from, SSACFG::BlockId _to)
	{
		auto const argIndex = _cfg.phiArgumentIndex(_from, _to);
		auto const& phis = _cfg.block(_to).phis;
		for (auto const& phiId: phis)
		{
			auto const& phiInfo = _cfg.valueInfo(phiId);
			yulAssert(std::holds_alternative<SSACFG::PhiValue>(phiInfo));
			auto const& phi = std::get<SSACFG::PhiValue>(phiInfo);
			m_reverseMapping[phiId] = phi.arguments[argIndex];
		}
	}

	bool empty() const { return m_reverseMapping.empty(); }
	StackSlot transform(StackSlot const& _slot) const;
	std::vector<StackSlot> transformStackToPhiValues(std::vector<StackSlot> const& _stack) const;

private:
	// maps phi_j -> v_i
	std::map<SSACFG::ValueId, SSACFG::ValueId> m_reverseMapping;
};
class Stack
{
public:
	explicit Stack(AbstractAssembly& _assembly, SSACFG const& _cfg, std::vector<StackSlot> const& _initialStack = {}):
		m_assembly(_assembly), m_cfg(_cfg), m_stack(_initialStack) {}
	Stack(Stack const&) = default;
	Stack(Stack&&) = default;
	Stack& operator=(Stack const&) = default;
	Stack& operator=(Stack&&) = default;

	size_t size() const { return m_stack.size(); }

	void permute(std::vector<StackSlot> const& _target);
	void createExactStack(std::vector<StackSlot> const& _target, PhiMapping const& _phis);

	void createStack(std::vector<StackSlot> const& _top, std::vector<StackSlot> const& _rest, PhiMapping const& _phis = {});

	StackSlot const& top() const
	{
		yulAssert(!empty(), "empty stack");
		return m_stack.back();
	}

	SSACFG::LiteralValue resolveLiteralValue(StackSlot const& _slot) const;
	void push(SSACFG::ValueId const& _value, bool _generateInstruction = true);
	void pop(bool _generateInstruction = true);
	void swap(size_t _depth, bool _generateInstruction = true);
	void dup(size_t _depth, bool _generateInstruction = true);
	bool dup(StackSlot const& _slot, bool _generateInstruction = true);
	bool empty() const;
	void clear();
	void bringUpSlot(StackSlot const& _slot);

	std::vector<StackSlot> const& data() const { return m_stack; }

	std::optional<size_t> slotIndex(StackSlot const& _slot) const;
private:
	std::reference_wrapper<AbstractAssembly> m_assembly;
	std::reference_wrapper<SSACFG const> m_cfg;
	std::vector<StackSlot> m_stack;
};
}

class SSACFGEVMCodeTransform
{
public:
	/// Use named labels for functions 1) Yes and check that the names are unique
	/// 2) For none of the functions 3) for the first function of each name.
	enum class UseNamedLabels { YesAndForceUnique, Never, ForFirstFunctionOfEachName };

	static std::vector<StackTooDeepError> run(
		AbstractAssembly& _assembly,
		ControlFlowLiveness const& _liveness,
		BuiltinContext& _builtinContext,
		UseNamedLabels _useNamedLabelsForFunctions
	);

private:
	using FunctionLabels = std::map<Scope::Function const*, AbstractAssembly::LabelID>;
	struct BlockData {
		bool generated = false;
		std::optional<AbstractAssembly::LabelID> label;
		std::optional<std::vector<ssacfg::StackSlot>> stackIn;
		//std::optional<std::vector<StackSlot>> stackOut;
	};

	static FunctionLabels registerFunctionLabels(
		AbstractAssembly& _assembly,
		ControlFlow const& _controlFlow,
		UseNamedLabels _useNamedLabelsForFunctions
	);

	SSACFGEVMCodeTransform(
		AbstractAssembly& _assembly,
		BuiltinContext& _builtinContext,
		FunctionLabels _functionLabels,
		SSACFG const& _cfg,
		SSACFGLiveness const& _liveness
	);

	bool requiresCleanStack(SSACFG::BlockId _block) const;
	void transformFunction(Scope::Function const& _function);

	void operator()(SSACFG::BlockId _block);
	void operator()(SSACFG::Operation const& _operation, std::set<SSACFG::ValueId> const& _liveOut);

	BlockData& blockData(SSACFG::BlockId const _block) { return m_blockData[_block.value]; }
	BlockData const& blockData(SSACFG::BlockId const _block) const { return m_blockData[_block.value]; }
	AbstractAssembly::LabelID functionLabel(Scope::Function const& _function) const
	{
		return m_functionLabels.at(&_function);
	}

	AbstractAssembly& m_assembly;
	BuiltinContext& m_builtinContext;
	SSACFG const& m_cfg;
	SSACFGLiveness const& m_liveness;
	std::vector<StackTooDeepError> m_stackErrors;
	FunctionLabels const m_functionLabels;
	ssacfg::Stack m_stack;
	std::vector<BlockData> m_blockData;
	SSACFG::BlockId m_currentBlock;
	std::vector<std::uint8_t> m_generatedBlocks;
};

}
