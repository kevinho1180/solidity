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

#include <libyul/backends/evm/SSAControlFlowGraph.h>
#include <libyul/backends/evm/SSACFGStackLayout.h>

namespace solidity::yul {

struct ControlFlowLiveness;
struct ControlFlow;
class SSACFGLiveness;
class ForwardSSACFGTopologicalSort;

class IsSSACFGLiteral
{
public:
	explicit IsSSACFGLiteral(SSACFG const& _cfg): m_cfg(_cfg) {}

	bool operator()(SSACFG::ValueId const _valueId) const { return m_cfg.isLiteralValue(_valueId); }
	bool operator()(SSACFGStackLayout::Slot const& _slot) const
	{
		return std::holds_alternative<SSACFG::ValueId>(_slot) && (*this)(std::get<SSACFG::ValueId>(_slot));
	}

private:
	SSACFG const& m_cfg;
};

class SSACFGStackLayoutGenerator {
public:
	static ControlFlowLayout generate(ControlFlowLiveness const& _controlFlowLiveness);
	static SSACFGStackLayout generate(SSACFGLiveness const& _cfgLiveness);
private:
	class RevertPaths
	{
	public:
		explicit RevertPaths(SSACFG const& _cfg, ForwardSSACFGTopologicalSort const& _topologicalSort);
		bool blockIsOnRevertPath(SSACFG::BlockId const& _blockId) const;
	private:
		std::vector<uint8_t> m_blockIsOnRevertPath;
	};

	explicit SSACFGStackLayoutGenerator(SSACFGLiveness const& _liveness);
	~SSACFGStackLayoutGenerator();

	bool requiresCleanStack(SSACFG::BlockId _block) const;

	SSACFGStackLayout const& run();
	void visitBlock(SSACFG::BlockId _blockId);
	SSACFGStackLayout::Stack visitOperation(
		SSACFG::BlockId _blockId,
		size_t _operationIndex,
		SSACFGStackLayout::Stack const& _inputStack
	);

	void populateBlockSuccessorStackIn(SSACFG::BlockId _blockId);
	void populateStackInFromJumpExit(SSACFG::BlockId _source, SSACFG::BasicBlock::Jump const& _jump);
	void populateStackInFromConditionalJumpExit(
		SSACFG::BlockId _source,
		SSACFG::BasicBlock::ConditionalJump const& _condJump
	);

	bool blockIsGenerated(SSACFG::BlockId _blockId) const;
	void markBlockGenerated(SSACFG::BlockId _blockId);
	bool blockHasDefinedStackIn(SSACFG::BlockId _blockId) const;
	void markBlockHasDefinedStackIn(SSACFG::BlockId _blockId);

	SSACFGLiveness const& m_liveness;
	SSACFG const& m_cfg;

	RevertPaths m_revertPaths;

	/// Keeping track of what blocks were already visited. uses uint8 over bool as there is no need to space-optimize.
	std::vector<std::uint8_t> m_generatedBlocks;
	/// Keeping track which block has its input layout defined
	std::vector<std::uint8_t> m_definedStackIn;
	SSACFGStackLayout m_stackLayout;
};

}
