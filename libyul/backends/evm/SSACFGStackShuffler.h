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

#include <libyul/backends/evm/StackHelpers.h>

#include <concepts>

namespace solidity::yul
{

template<typename StackShuffler>
concept SSACFGStackShuffler = requires(
	StackShuffler _shuffler,
	typename StackShuffler::Stack _sourceStack,
	typename StackShuffler::Stack _targetStackTop,
	std::vector<typename StackShuffler::Stack::Slot> _targetStackRest,
	typename StackShuffler::Stack::Slot _slot
)
{
	typename StackShuffler::Stack;
	{ _shuffler.canBeFreelyGenerated(_slot) } -> std::same_as<bool>;
	{ _shuffler.shuffle(_sourceStack, _targetStackTop, _targetStackRest) } -> std::convertible_to<typename StackShuffler::Stack>;
};

template<SSACFGStack StackType>
struct BubbleShuffler
{
	using Stack = StackType;
	using StackSlot = typename Stack::Slot;
	static Stack shuffle(Stack const& _sourceStack, Stack const& _targetStackTop, std::vector<StackSlot> const& _targetStackRest)
	{
		Stack shuffledStack = _sourceStack;
		auto const histogram = [](Stack const& _stack, std::vector<StackSlot> const& _rest = {})
		{
			std::map<StackSlot, size_t> counts;
			for (auto const& slot: _stack)
				++counts[slot];
			for (auto const& slot: _rest)
				++counts[slot];
			return counts;
		};
		auto const targetCounts = histogram(_targetStackTop, _targetStackRest);
		{
			auto const stackCounts = histogram(_sourceStack);
			// first, remove everything from the stack that occurs more often than what's in the target
			for (auto const& [slot, count]: stackCounts)
			{
				size_t targetCount = 0;
				if (auto it = targetCounts.find(slot); it != targetCounts.end())
					targetCount = it->second;
				if (count > targetCount)
					for (size_t i = 0; i < count - targetCount; ++i)
					{
						auto depth = util::findOffset(_sourceStack | ranges::views::reverse, slot);
						yulAssert(depth);
						if (depth > 0)
							shuffledStack.swap(*depth);
						shuffledStack.pop();
					}
			}
			// then dup/push stuff that's not there yet in appropriate quantities
			for (auto const& [slot, targetCount]: targetCounts)
			{
				auto findIt = stackCounts.find(slot);
				if (findIt == stackCounts.end())
					for (size_t i = 0; i < targetCount; ++i)
						shuffledStack.bringUpSlot(slot);
				else
				{
					auto currentCount = std::min(targetCount, findIt->second);
					yulAssert(currentCount <= targetCount);
					for (size_t i = 0; i < targetCount - currentCount; ++i)
					{
						auto const depth = shuffledStack.slotIndex(slot);
						yulAssert(depth);
						shuffledStack.dup(*depth);
					}
				}
			}
		}

		// now we have the same elements in the shuffled stack - just potentially in a different order
		yulAssert(histogram(shuffledStack) == targetCounts);
		auto const targetStackTopOffset = _targetStackRest.size();
		for (size_t i = 0; i < _targetStackTop.size(); ++i)
		{
			// look at the bottom element of the stack and swap something there if it's not already the correct slot
			if (shuffledStack[i + targetStackTopOffset] != _targetStackTop[i])
			{
				auto const depth = util::findOffset(shuffledStack | ranges::views::reverse, _targetStackTop[i]);
				if (depth > 0)
					shuffledStack.swap(*depth);
				yulAssert(shuffledStack.top() == _targetStackTop[i]);
				if (shuffledStack.size() > i + targetStackTopOffset + 1)
					shuffledStack.swap(shuffledStack.size() - 1 - targetStackTopOffset - i);
			}
			yulAssert(shuffledStack[i + targetStackTopOffset] == _targetStackTop[i]);
		}

		yulAssert(shuffledStack.size() == _targetStackTop.size() + _targetStackRest.size());
		// yulAssert(m_stack == _target, fmt::format("Stack target mismatch: current = {} =/= {} = target", stackToStringLoc(m_cfg.get(), m_stack), stackToStringLoc(m_cfg.get(), _target)));
		return shuffledStack;
	}
	static bool canBeFreelyGenerated(StackSlot const&) { return false; }

};

template<SSACFGStack StackType>
struct DanielShuffler
{
	using Stack = StackType;
	using StackSlot = typename Stack::Slot;
	static Stack shuffle(Stack const& _sourceStack, Stack const& _targetStackTop, Stack const& _targetStackRest)
	{
		struct ShuffleOperations
		{
			Stack& currentStack;
			std::map<StackSlot, size_t> sourceCounts;
			Stack const& targetStack;
			std::map<StackSlot, size_t> targetCounts;

			ShuffleOperations(
				Stack& _currentStack,
				Stack const& _targetStack
			): currentStack(_currentStack), targetStack(_targetStack)
			{
				auto const histogram = [](Stack const& _stack)
				{
					std::map<StackSlot, size_t> counts;
					for (auto const& targetSlot: _stack)
						++counts[targetSlot];
					return counts;
				};
				targetCounts = histogram(targetStack);
				sourceCounts = histogram(currentStack);
			}

			bool isCompatible(size_t _source, size_t _target) const
			{
				return _source < currentStack.size() && _target < targetStack.size() && currentStack[_source] == targetStack[_target];
			}

			bool sourceIsSame(size_t _sourceOffset1, size_t _sourceOffset2) const
			{
				return _sourceOffset1 < currentStack.size() && _sourceOffset2 < currentStack.size() && currentStack[_sourceOffset1] == currentStack[_sourceOffset2];
			}

			int sourceMultiplicity(size_t _sourceOffset) const
			{
				auto const& slot = currentStack[_sourceOffset];
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
				// todo integrate "canBeFreelyGenerated" or "bringUpSlot" for more high-level functionality
				currentStack.push(targetStack[_targetOffset]);
			}

		};
		Stack shuffledStack = _sourceStack;
		Stack targetStack(_targetStackRest);
		targetStack.pushAll(_targetStackTop);
		Shuffler<ShuffleOperations>::shuffle(shuffledStack, targetStack);
		return shuffledStack;
	}
};

}
