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

#include <libyul/backends/evm/SSACFGStackLayoutGenerator.h>

#include <libyul/backends/evm/ControlFlow.h>
#include <libyul/backends/evm/SSACFGLiveness.h>
#include <libyul/backends/evm/SSACFGStackShuffler.h>

#include <libsolutil/Visitor.h>

#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/none_of.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/reverse.hpp>

#include <range/v3/to_container.hpp>
#include <range/v3/view/drop.hpp>

using namespace solidity::yul;

namespace
{
class IsLiteral
{
public:
	explicit IsLiteral(SSACFG const& _cfg): m_cfg(_cfg) {}

	bool operator()(SSACFG::ValueId const _valueId) const { return m_cfg.isLiteralValue(_valueId); }
	bool operator()(SSACFGStackLayout::Slot const& _slot) const
	{
		return std::holds_alternative<SSACFG::ValueId>(_slot) && (*this)(std::get<SSACFG::ValueId>(_slot));
	}

private:
	SSACFG const& m_cfg;
};

/// Checks whether the operation is builtin and if it is considered commutative in the first two arguments.
/*bool operationIsTwoCommutativeBuiltin(SSACFG::Operation const& _operation)
{
	if (!std::holds_alternative<SSACFG::BuiltinCall>(_operation.kind))
		return false;

	BuiltinFunction const& builtin = std::get<SSACFG::BuiltinCall>(_operation.kind).builtin.get();
	static std::array constexpr twoCommutativeBuiltins{"add", "addmod", "mul", "mulmod", "eq", "and", "or", "xor"};
	return ranges::find(twoCommutativeBuiltins, builtin.name) != twoCommutativeBuiltins.end();
}*/

struct StackShuffleResult
{
	SSACFGStackLayout::Stack outputStack;
	std::optional<size_t> estimatedGasCosts = std::nullopt;
};

/// shuffles `_source` stack to the point where the `_requiredTop` of the `_target` is met and the rest is in arbitrary order
template<SSACFGStackShuffler Shuffler>
StackShuffleResult shuffleStack(
	SSACFGStackLayout::Stack const& _source,
	SSACFGStackLayout::Stack const& _targetTop,
	std::set<SSACFGStackLayout::Slot> const& _targetRest,
	ReversePhiFunctionTransform const& _phiFunctionTransform = {}
)
{
	// todo
	//return {_target};
	return {};
}

}

ControlFlowLayout SSACFGStackLayoutGenerator::generate(ControlFlowLiveness const& _controlFlowLiveness)
{
	ControlFlowLayout layout;
	layout.mainLayout = SSACFGStackLayoutGenerator{*_controlFlowLiveness.mainLiveness}.run();

	layout.functionLayouts.reserve(_controlFlowLiveness.functionLiveness.size());
	for (auto const& functionLiveness: _controlFlowLiveness.functionLiveness)
		layout.functionLayouts.push_back(SSACFGStackLayoutGenerator{*functionLiveness}.run());

	return layout;
}

SSACFGStackLayoutGenerator::SSACFGStackLayoutGenerator(
	SSACFGLiveness const& _liveness
):
	m_liveness(_liveness),
	m_cfg(_liveness.cfg()),
	m_generatedBlocks(_liveness.cfg().numBlocks(), false)
{
	m_stackLayout.blockLayouts.resize(m_cfg.numBlocks());
	if (!m_cfg.function)
		// for the main CFG: empty initial stack
		m_stackLayout[m_cfg.entry].stackIn = {};
	else
		// for function CFG: arguments are at the top of the stack
		m_stackLayout[m_cfg.entry].stackIn = SSACFGStackLayout::Stack(
			m_cfg.arguments |
			ranges::views::reverse |
			ranges::views::transform([](auto&& _variableAndValueId) -> SSACFGStackLayout::Slot { return std::get<1>(_variableAndValueId); }) |
			ranges::to<std::vector>
		);
}

SSACFGStackLayoutGenerator::~SSACFGStackLayoutGenerator() = default;

bool SSACFGStackLayoutGenerator::requiresCleanStack(SSACFG::BlockId const _block) const
{
	// todo extend to cut edges
	util::GenericVisitor constexpr exitVisitor{
		[&](SSACFG::BasicBlock::MainExit const&) { return false; },
		[&](SSACFG::BasicBlock::Terminated const&){ return false; },
		[](auto const&) { return true; }
	};
	return std::visit(exitVisitor, m_cfg.block(_block).exit);
}

SSACFGStackLayout const& SSACFGStackLayoutGenerator::run()
{
	for (auto const& blockIdValue: m_liveness.topologicalSort().preOrder())
		visitBlock(SSACFG::BlockId{blockIdValue});
	return m_stackLayout;
}


void SSACFGStackLayoutGenerator::visitBlock(SSACFG::BlockId const _blockId)
{
	yulAssert(!blockIsGenerated(_blockId));
	yulAssert(blockHasDefinedStackIn(_blockId));

	SSACFGStackLayout::Stack currentStack = m_stackLayout[_blockId].stackIn;
	for (size_t operationIndex = 0; operationIndex < m_cfg.block(_blockId).operations.size(); ++operationIndex)
		currentStack = visitOperation(_blockId, operationIndex, currentStack);
	m_stackLayout[_blockId].stackOut = currentStack;

	markBlockGenerated(_blockId);
	populateBlockExitStackIn(_blockId);
}

SSACFGStackLayout::Stack SSACFGStackLayoutGenerator::visitOperation(
	SSACFG::BlockId const _blockId,
	size_t const _operationIndex,
	SSACFGStackLayout::Stack const& _inputStack
)
{
	yulAssert(_operationIndex < m_cfg.block(_blockId).operations.size());
	auto const& operation = m_cfg.block(_blockId).operations[_operationIndex];
	auto const& operationLiveOut = m_liveness.operationsLiveOut(_blockId)[_operationIndex];

	// literals should have been pulled out a priori and now are treated as push constants
	yulAssert(ranges::none_of(operationLiveOut, IsLiteral(m_cfg)));

	auto const liveOutWithoutOutputsSet = operationLiveOut - operation.outputs;
	auto const liveOutWithoutOutputs = std::vector<SSACFGStackLayout::Slot>(liveOutWithoutOutputsSet.begin(), liveOutWithoutOutputsSet.end());
	SSACFGStackLayout::Stack const requiredStackTop(std::vector<SSACFGStackLayout::Slot>(operation.inputs.begin(), operation.inputs.end()));

	// todo if we don't require a clean stack, we might as well just bring up the args and leave the rest as-is
	static_assert(SSACFGStackShuffler<BubbleShuffler<SSACFGStackLayout::Stack>>, "Bubble shuffler conforms to SSACFGStackShuffler concept.");
	// auto outputStack = BubbleShuffler<SSACFGStackLayout::Stack>::shuffle(_inputStack, requiredStackTop, liveOutWithoutOutputs);
	// todo for now I just require the stack top to be the ops inputs and leave the rest as-is
	m_stackLayout[_blockId].operationIn[_operationIndex] = SSACFGStackLayout::Stack(_inputStack.data + requiredStackTop.data);
	auto stackOut = BubbleShuffler<SSACFGStackLayout::Stack>::shuffle(_inputStack, requiredStackTop, _inputStack.data);
	// compress
	{
		while (stackOut.size() > 0)
		{
			if (IsLiteral(m_cfg)(stackOut.top()))
				stackOut.pop();
			else if (auto offset = util::findOffset(stackOut | ranges::views::reverse | ranges::views::drop(1), stackOut.top()))
			{
				if (*offset + 2 < 16) // todo non-eof specific and why +2?
					stackOut.pop();
				else
					break;
			}
			else
				break;
		}
	}
	return stackOut;
}


void SSACFGStackLayoutGenerator::populateBlockExitStackIn(SSACFG::BlockId const _blockId)
{
	std::visit(util::GenericVisitor{
		[](SSACFG::BasicBlock::MainExit const&) {},
		[&](SSACFG::BasicBlock::Jump const& _jump)
		{
			populateStackInFromJumpExit(_blockId, _jump);
		},
		[&](SSACFG::BasicBlock::ConditionalJump const& _condJump)
		{
			populateStackInFromConditionalJumpExit(_blockId, _condJump);
		},
		[](SSACFG::BasicBlock::JumpTable const&)
		{
			yulAssert(false, "nope, not yet"); // todo
		},
		[](SSACFG::BasicBlock::FunctionReturn const&) {},
		[](SSACFG::BasicBlock::Terminated const&) {}
	}, m_cfg.block(_blockId).exit);
}


void SSACFGStackLayoutGenerator::populateStackInFromJumpExit(
	SSACFG::BlockId const _source, SSACFG::BasicBlock::Jump const& _jump)
{
	if (blockHasDefinedStackIn(_jump.target))
		return;

	auto const& targetLiveIn = m_liveness.liveIn(_jump.target);
	yulAssert(ranges::none_of(targetLiveIn, IsLiteral(m_cfg)));

	std::vector<SSACFGStackLayout::Slot> const targetLiveInSlots(targetLiveIn.begin(), targetLiveIn.end());
	if (requiresCleanStack(_jump.target))
		m_stackLayout[_jump.target].stackIn = SSACFGStackLayout::Stack(targetLiveInSlots);
	else
		m_stackLayout[_jump.target].stackIn = SSACFGStackLayout::Stack(m_stackLayout[_source].stackOut.data + targetLiveInSlots);
	markBlockHasDefinedStackIn(_jump.target);
}

void SSACFGStackLayoutGenerator::populateStackInFromConditionalJumpExit(
	SSACFG::BlockId const _source,
	SSACFG::BasicBlock::ConditionalJump const& _condJump
)
{
	if (blockHasDefinedStackIn(_condJump.nonZero) && blockHasDefinedStackIn(_condJump.zero))
		return;

	if (!blockHasDefinedStackIn(_condJump.nonZero))
	{
		auto const& zeroLiveIn = m_liveness.liveIn(_condJump.zero);
		yulAssert(ranges::none_of(zeroLiveIn, IsLiteral(m_cfg)));
		auto const& nonZeroLiveIn = m_liveness.liveIn(_condJump.nonZero);
		yulAssert(ranges::none_of(nonZeroLiveIn, IsLiteral(m_cfg)));

		auto const pulledBackZeroLiveIn = zeroLiveIn | ranges::views::transform(ReversePhiFunctionTransform(m_cfg, _source, _condJump.zero)) | ranges::to<std::set>;

		std::vector<SSACFGStackLayout::Slot> const nonZeroLiveInSlots(nonZeroLiveIn.begin(), nonZeroLiveIn.end());
		auto const remainingZeroLiveIn = pulledBackZeroLiveIn - nonZeroLiveIn;
		std::vector<SSACFGStackLayout::Slot> const remainingZeroLiveInSlots(remainingZeroLiveIn.begin(), remainingZeroLiveIn.end());

		if (requiresCleanStack(_condJump.nonZero))
			// [phi^-1(liveInZero) - liveInNonZero, liveInNonZero]
			m_stackLayout[_condJump.nonZero].stackIn = SSACFGStackLayout::Stack(remainingZeroLiveInSlots + nonZeroLiveInSlots);
		else
			m_stackLayout[_condJump.nonZero].stackIn = SSACFGStackLayout::Stack(m_stackLayout[_source].stackOut.data + nonZeroLiveInSlots);

		// condition always has to be at the top
		m_stackLayout[_condJump.nonZero].stackIn.push(_condJump.condition);

		markBlockHasDefinedStackIn(_condJump.nonZero);
	}

	if (!blockHasDefinedStackIn(_condJump.zero))
	{
		auto const& zeroLiveIn = m_liveness.liveIn(_condJump.zero);
		yulAssert(ranges::none_of(zeroLiveIn, IsLiteral(m_cfg)));

		std::vector<SSACFGStackLayout::Slot> const zeroLiveInStackData(zeroLiveIn.begin(), zeroLiveIn.end());
		if (requiresCleanStack(_condJump.zero))
			m_stackLayout[_condJump.zero].stackIn = SSACFGStackLayout::Stack(zeroLiveInStackData);
		else
			m_stackLayout[_condJump.zero].stackIn = SSACFGStackLayout::Stack(m_stackLayout[_source].stackOut.data + zeroLiveInStackData);
		markBlockHasDefinedStackIn(_condJump.zero);
	}
}

bool SSACFGStackLayoutGenerator::blockIsGenerated(SSACFG::BlockId const _blockId) const
{
	return m_generatedBlocks[_blockId.value];
}

void SSACFGStackLayoutGenerator::markBlockGenerated(SSACFG::BlockId const _blockId)
{
	m_generatedBlocks[_blockId.value] = true;
}

bool SSACFGStackLayoutGenerator::blockHasDefinedStackIn(SSACFG::BlockId const _blockId) const
{
	return m_definedStackIn[_blockId.value];
}

void SSACFGStackLayoutGenerator::markBlockHasDefinedStackIn(SSACFG::BlockId const _blockId)
{
	m_definedStackIn[_blockId.value] = true;
}

