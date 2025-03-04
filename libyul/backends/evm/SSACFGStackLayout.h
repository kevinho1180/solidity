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

#include <libyul/Exceptions.h>

#include <libsolutil/Visitor.h>

#include <range/v3/view/reverse.hpp>

#include <map>
#include <variant>
#include <vector>

namespace solidity::yul
{
/// If Block `_from` -> Block `_to` and `_to` has phi functions `v_k := phi(..., _from => v_i, ...)`, this transform
/// pulls values `v_k` back to `v_i`.
class ReversePhiFunctionTransform
{
public:
	ReversePhiFunctionTransform() = default;
	ReversePhiFunctionTransform(SSACFG const& _cfg, SSACFG::BlockId _from, SSACFG::BlockId _to);

	/// whether the transform is guaranteed to be a no-op, ie, there is no phi function in `_to`
	bool noOp() const;
	SSACFG::ValueId operator()(SSACFG::ValueId _valueId) const;

	// if we have a variant with value id contained in the type union
	template<typename... T>
	std::variant<T...> operator()(std::variant<T...> const& _someSlot) const
	{
		static bool constexpr variantContainsValueId = std::disjunction_v<std::is_same<SSACFG::ValueId, T>...>;
		static_assert(variantContainsValueId);
		if (auto valueId = std::get_if<SSACFG::ValueId>(&_someSlot))
			return (*this)(*valueId);
		return _someSlot;
	}

private:
	std::map<SSACFG::ValueId, SSACFG::ValueId> m_reversePhiMap = {};
};

template<typename Stack>
concept SSACFGStack = requires(Stack _stack, Stack _otherStack, size_t _depth, typename Stack::Slot _slot)
{
	typename Stack::Slot;
	{ _stack.top() } -> std::convertible_to<typename Stack::Slot>;
	{ _stack.swap(_depth) } -> std::same_as<void>;
	{ _stack.pop() } -> std::same_as<void>;
	{ _stack.push(_slot) } -> std::same_as<void>;
	{ _stack.pushOrDup(_slot) } -> std::same_as<void>;
	{ _stack.slotIndex(_slot) } -> std::convertible_to<std::optional<size_t>>;
	{ _stack.size() } -> std::convertible_to<size_t>;
	{ _stack[_depth] } -> std::convertible_to<typename Stack::Slot>;
	{ _stack.pushAll(_otherStack) } -> std::same_as<void>;
	// we can iterate over a stack
	{ ranges::range<ranges::range_value_t<Stack>> };
	{ ranges::range_value_t<Stack>{} } -> std::convertible_to<typename Stack::Slot>;
};

class SSACFGStackLayoutStack
{
public:
	using Slot = std::variant<SSACFG::ValueId, AbstractAssembly::LabelID>;

	SSACFGStackLayoutStack() = default;
	explicit SSACFGStackLayoutStack(std::vector<Slot> _stack): m_data(std::move(_stack)) {}

	void swap(size_t const _depth)
	{
		yulAssert(m_data.size() > _depth);
		std::swap(m_data[m_data.size() - _depth - 1], m_data.back());
	}

	void pop()
	{
		yulAssert(!m_data.empty());
		m_data.pop_back();
	}

	void push(Slot const& _value)
	{
		m_data.emplace_back(_value);
	}

	void dup(size_t const _depth)
	{
		yulAssert(m_data.size() >= _depth + 1);
		m_data.push_back(m_data[m_data.size() - _depth - 1]);
	}

	bool dup(Slot const& _value)
	{
		auto offset = slotIndex(_value);
		if (offset)
			dup(*offset);
		return offset.has_value();
	}

	std::optional<size_t> slotIndex(Slot const& _value) const
	{
		auto const offset = util::findOffset(m_data | ranges::views::reverse, _value);
		if (offset)
		{
			yulAssert(m_data.size() >= *offset + 1);
			yulAssert(m_data[m_data.size() - *offset - 1] == _value);
		}
		return offset;
	}

	void bringUpSlot(Slot const& _slot)
	{
		std::visit(util::GenericVisitor{
			[&](SSACFG::ValueId _value) {
				if (!dup(_slot))
					push(_value);
			},
			[&](AbstractAssembly::LabelID _label) {
				m_data.emplace_back(_label);
			}
		}, _slot);
	}

	void pushOrDup(Slot const& _slot)
	{
		bringUpSlot(_slot);
	}

	size_t size() const
	{
		return m_data.size();
	}

	Slot const& operator[](size_t const _index) const
	{
		return m_data[_index];
	}

	Slot const& top() const
	{
		yulAssert(!m_data.empty());
		return m_data.back();
	}

	void pushAll(SSACFGStackLayoutStack const& _other)
	{
		m_data += _other.m_data;
	}

	auto begin() const { return ranges::begin(m_data); }
	auto end() const { return ranges::end(m_data); }

	auto operator<=>(SSACFGStackLayoutStack const&) const = default;

	std::vector<Slot> const& stackData() const { return m_data; }
private:
	std::vector<Slot> m_data;
};

static_assert(SSACFGStack<SSACFGStackLayoutStack>);

struct SSACFGStackLayout
{
	// each operation has a current stack
	using Stack = SSACFGStackLayoutStack;
	// a slot can be some valueId or a labelId
	using Slot = Stack::Slot;

	// Each block has its own layout
	struct BlockLayout
	{
		// stack layout required to enter the block
		Stack stackIn;
		// stack layout required to execute the i-th operation in the block
		std::vector<Stack> operationIn;
		// stack after the block was executed
		Stack stackOut;
	};

	// each block has a fixed list of operations
	using BlockLayouts = std::vector<BlockLayout>;

	BlockLayout& operator[](SSACFG::BlockId const _blockId)
	{
		yulAssert(_blockId.value < blockLayouts.size());
		return blockLayouts[_blockId.value];
	}

	BlockLayout const& operator[](SSACFG::BlockId const _blockId) const
	{
		yulAssert(_blockId.value < blockLayouts.size());
		return blockLayouts[_blockId.value];
	}

	BlockLayouts blockLayouts;
};

struct ControlFlowLayout
{
	SSACFGStackLayout mainLayout;
	std::vector<SSACFGStackLayout> functionLayouts;
};
}
