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

#include <libyul/optimiser/ASTWalker.h>
#include <liblangutil/DebugData.h>
#include <libyul/AST.h>

namespace solidity::yul
{
class SourceLocationRemover: public ASTModifier
{
public:
	void operator()(Literal& _literal) override
	{
		resetDebugData(_literal);
		ASTModifier::operator()(_literal);
	}
	void operator()(Identifier& _identifier) override
	{
		resetDebugData(_identifier);
		ASTModifier::operator()(_identifier);
	}
	void operator()(FunctionCall& _funCall) override
	{
		resetDebugData(_funCall.functionName);
		resetDebugData(_funCall);
		ASTModifier::operator()(_funCall);
	}
	void operator()(ExpressionStatement& _statement) override
	{
		resetDebugData(_statement);
		ASTModifier::operator()(_statement);
	}
	void operator()(Assignment& _assignment) override
	{
		resetDebugData(_assignment);
		ASTModifier::operator()(_assignment);
	}
	void operator()(VariableDeclaration& _varDecl) override
	{
		resetDebugData(_varDecl);
		ASTModifier::operator()(_varDecl);
	}
	void operator()(If& _if) override
	{
		resetDebugData(_if);
		ASTModifier::operator()(_if);
	}
	void operator()(Switch& _switch) override
	{
		resetDebugData(_switch);
		ASTModifier::operator()(_switch);
	}
	void operator()(FunctionDefinition& _functionDefinition) override
	{
		resetDebugData(_functionDefinition);
		ASTModifier::operator()(_functionDefinition);
	}
	void operator()(ForLoop& _forLoop) override
	{
		resetDebugData(_forLoop);
		ASTModifier::operator()(_forLoop);
	}
	void operator()(Break& _break) override
	{
		resetDebugData(_break);
		ASTModifier::operator()(_break);
	}
	void operator()(Continue& _continue) override
	{
		resetDebugData(_continue);
		ASTModifier::operator()(_continue);
	}
	void operator()(Leave& _leaveStatement) override
	{
		resetDebugData(_leaveStatement);
		ASTModifier::operator()(_leaveStatement);
	}
	void operator()(Block& _block) override
	{
		resetDebugData(_block);
		ASTModifier::operator()(_block);
	}
	void visit(Statement& _st) override
	{
		resetDebugData(_st);
		ASTModifier::visit(_st);
	}
	void visit(Expression& _e) override
	{
		resetDebugData(_e);
		ASTModifier::visit(_e);
	}
	template<typename T>
	void resetDebugData(T& _node)
	{
		setDebugData(_node, {});
	}

};
}
