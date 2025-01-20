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

#include <libyul/backends/evm/AbstractAssembly.h>
#include <libyul/backends/evm/SSAControlFlowGraph.h>

#include <variant>

namespace solidity::yul {

struct ControlFlowLiveness;
struct ControlFlow;
class SSACFGLiveness;

struct SSACFGStackLayout
{
	// a slot can be some valueId or a labelId
	using Slot = std::variant<SSACFG::ValueId, AbstractAssembly::LabelID>;
	// each operation has a current stack
	using Stack = std::vector<Slot>;
	// each block has a fixed list of operations
	using BlockLayouts = std::vector<Stack>;

	// layout for the main graph
	BlockLayouts mainLayout;
	// layout for each function graph, ordered as in the `ControlFlow`
	std::vector<BlockLayouts> functionLayouts;
};

class SSACFGStackLayoutGenerator {
public:
	static SSACFGStackLayout run(ControlFlowLiveness const& _controlFlowLiveness);
private:
	SSACFGStackLayoutGenerator(SSACFGLiveness const& _liveness, SSACFGStackLayout::Stack const& _initialStack = {});
	~SSACFGStackLayoutGenerator();

	SSACFGStackLayout::BlockLayouts processEntryPoint();

	bool requiresCleanStack(SSACFG::BlockId _block) const;

	SSACFGLiveness const& m_liveness;
	SSACFG const& m_cfg;

	SSACFGStackLayout::BlockLayouts m_layout;
};

}
