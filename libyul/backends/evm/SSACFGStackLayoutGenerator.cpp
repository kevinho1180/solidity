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

ControlFlowLayout SSACFGStackLayoutGenerator::generate(ControlFlowLiveness const& _controlFlowLiveness)
{
	ControlFlowLayout layout;
	layout.mainLayout = generate(*_controlFlowLiveness.mainLiveness);

	layout.functionLayouts.reserve(_controlFlowLiveness.functionLiveness.size());
	for (auto const& functionLiveness: _controlFlowLiveness.functionLiveness)
		layout.functionLayouts.push_back(generate(*functionLiveness));

	return layout;
}

SSACFGStackLayout SSACFGStackLayoutGenerator::generate(SSACFGLiveness const& _cfgLiveness)
{
	return SSACFGStackLayoutGenerator{_cfgLiveness}.run();
}

SSACFGStackLayoutGenerator::SSACFGStackLayoutGenerator(
	SSACFGLiveness const& _liveness
):
	m_liveness(_liveness),
	m_cfg(_liveness.cfg()),
	m_generatedBlocks(_liveness.cfg().numBlocks(), false),
	m_definedStackIn(_liveness.cfg().numBlocks(), false)
{
	m_stackLayout.blockLayouts.resize(m_cfg.numBlocks());
	if (!m_cfg.function)
	{
		// for the main CFG: empty initial stack
		m_stackLayout[m_cfg.entry].stackIn = {};
		markBlockHasDefinedStackIn(m_cfg.entry);
	}
	else
	{
		// for function CFG: arguments are at the top of the stack
		m_stackLayout[m_cfg.entry].stackIn = SSACFGStackLayout::Stack(
			m_cfg.arguments |
			ranges::views::reverse |
			ranges::views::transform([](auto&& _variableAndValueId) -> SSACFGStackLayout::Slot { return std::get<1>(_variableAndValueId); }) |
			ranges::to<std::vector>
		);
		markBlockHasDefinedStackIn(m_cfg.entry);
	}
}

SSACFGStackLayoutGenerator::~SSACFGStackLayoutGenerator() = default;

bool SSACFGStackLayoutGenerator::requiresCleanStack(SSACFG::BlockId const _block) const
{
	return true;
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
	auto const numOperationsInBlock = m_cfg.block(_blockId).operations.size();
	m_stackLayout[_blockId].operationIn.resize(numOperationsInBlock);
	for (size_t operationIndex = 0; operationIndex < numOperationsInBlock; ++operationIndex)
		currentStack = visitOperation(_blockId, operationIndex, currentStack);
	m_stackLayout[_blockId].stackOut = currentStack;

	markBlockGenerated(_blockId);
	populateBlockSuccessorStackIn(_blockId);
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
	yulAssert(ranges::none_of(operationLiveOut, IsSSACFGLiteral(m_cfg)));

	auto const liveOutWithoutOutputsSet = operationLiveOut - operation.outputs;
	auto const liveOutWithoutOutputs = std::set<SSACFGStackLayout::Slot>(liveOutWithoutOutputsSet.begin(), liveOutWithoutOutputsSet.end());
	std::vector<SSACFGStackLayout::Slot> requiredStackTop;
	if (auto const* call = std::get_if<SSACFG::Call>(&operation.kind))
		if (call->canContinue)
			requiredStackTop.emplace_back(SSACFGFunctionReturnLabel{&call->call.get()});
	requiredStackTop += operation.inputs;

	// todo if we don't require a clean stack, we might as well just bring up the args and leave the rest as-is
	static_assert(SSACFGStackShuffler<BubbleShuffler<SSACFGStackLayout::Stack>>, "Bubble shuffler conforms to SSACFGStackShuffler concept.");
	static_assert(SSACFGStackShuffler<DanielShuffler<SSACFGStackLayout::Stack>>, "Daniel shuffler conforms to SSACFGStackShuffler concept.");
	// auto outputStack = BubbleShuffler<SSACFGStackLayout::Stack>::shuffle(_inputStack, requiredStackTop, liveOutWithoutOutputs);
	// auto stackOut = BubbleShuffler<SSACFGStackLayout::Stack>::shuffle(_inputStack, requiredStackTop, _inputStack.data);
	auto stack = DanielShuffler<SSACFGStackLayout::Stack>::shuffle(_inputStack, liveOutWithoutOutputs, requiredStackTop);
	m_stackLayout[_blockId].operationIn[_operationIndex] = stack;

	for (size_t i = 0; i < requiredStackTop.size(); ++i)
		stack.pop();
	for (auto const& val: operation.outputs)
		stack.push(val);
	return stack;
}

// todo better name here :)
void SSACFGStackLayoutGenerator::populateBlockSuccessorStackIn(SSACFG::BlockId const _blockId)
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
		[](SSACFG::BasicBlock::FunctionReturn const&)
		{

		},
		[](SSACFG::BasicBlock::Terminated const&) {}
	}, m_cfg.block(_blockId).exit);
}


void SSACFGStackLayoutGenerator::populateStackInFromJumpExit(
	SSACFG::BlockId const _source,
	SSACFG::BasicBlock::Jump const& _jump
)
{
	if (blockHasDefinedStackIn(_jump.target))
		return;

	auto const& targetLiveIn = m_liveness.liveIn(_jump.target);
	yulAssert(ranges::none_of(targetLiveIn, IsSSACFGLiteral(m_cfg)));

	// todo: just pop the stuff from stackOut we don't need and use that as stackIn
	std::set<SSACFGStackLayout::Slot> const targetLiveInSlots(targetLiveIn.begin(), targetLiveIn.end());
	if (requiresCleanStack(_jump.target))
	{
		// m_stackLayout[_jump.target].stackIn = DanielShuffler<SSACFGStackLayout::Stack>::shuffle(m_stackLayout[_source].stackOut, targetLiveInSlots, {});
		m_stackLayout[_jump.target].stackIn = BlockStackInShuffler<SSACFGStackLayout::Stack>::shuffle(m_stackLayout[_source].stackOut, targetLiveInSlots);
		yulAssert(std::set(m_stackLayout[_jump.target].stackIn.begin(), m_stackLayout[_jump.target].stackIn.end()) == targetLiveInSlots);
	}
	else
	{
		yulAssert(false);
	}
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
		yulAssert(ranges::none_of(zeroLiveIn, IsSSACFGLiteral(m_cfg)));
		auto const& nonZeroLiveIn = m_liveness.liveIn(_condJump.nonZero);
		yulAssert(ranges::none_of(nonZeroLiveIn, IsSSACFGLiteral(m_cfg)));

		auto const pulledBackZeroLiveIn = zeroLiveIn | ranges::views::transform(ReversePhiFunctionTransform(m_cfg, _source, _condJump.zero)) | ranges::to<std::set>;

		std::vector<SSACFGStackLayout::Slot> const nonZeroLiveInSlots(nonZeroLiveIn.begin(), nonZeroLiveIn.end());
		auto const remainingZeroLiveIn = pulledBackZeroLiveIn - nonZeroLiveIn;
		std::vector<SSACFGStackLayout::Slot> const remainingZeroLiveInSlots(remainingZeroLiveIn.begin(), remainingZeroLiveIn.end());

		// todo use shuffle algo
		if (requiresCleanStack(_condJump.nonZero))
			// [phi^-1(liveInZero) - liveInNonZero, liveInNonZero]
			m_stackLayout[_condJump.nonZero].stackIn = SSACFGStackLayout::Stack(remainingZeroLiveInSlots + nonZeroLiveInSlots);
		else
		{
			yulAssert(false);
		}

		markBlockHasDefinedStackIn(_condJump.nonZero);
	}

	if (!blockHasDefinedStackIn(_condJump.zero))
	{
		auto const& zeroLiveIn = m_liveness.liveIn(_condJump.zero);
		yulAssert(ranges::none_of(zeroLiveIn, IsSSACFGLiteral(m_cfg)));

		std::vector<SSACFGStackLayout::Slot> const zeroLiveInStackData(zeroLiveIn.begin(), zeroLiveIn.end());
		// todo use shuffle algo
		if (requiresCleanStack(_condJump.zero))
			m_stackLayout[_condJump.zero].stackIn = SSACFGStackLayout::Stack(zeroLiveInStackData);
		else
		{
			yulAssert(false);
		}
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

