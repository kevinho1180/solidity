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


#include <functional>
#include <list>
#include <set>

namespace solidity::util
{

/**
 * Detector for cycles in directed graphs. It returns the first
 * vertex on the path towards a cycle or a nullptr if there is
 * no reachable cycle starting from a given vertex.
 */
template <typename V>
class CycleDetector
{
public:
	using Visitor = std::function<void(V const&, CycleDetector&, size_t)>;

	/// Initializes the cycle detector
	/// @param _visit function that is given the current vertex
	///               and is supposed to call @a run on all
	///               adjacent vertices.
	explicit CycleDetector(Visitor _visit):
		m_visit(std::move(_visit))
	{  }

	/// Recursively perform cycle detection starting
	/// (or continuing) with @param _vertex
	/// @returns the first vertex on the path towards a cycle from @a _vertex
	/// or nullptr if no cycle is reachable from @a _vertex.
	V const* run(V const& _vertex)
	{
		if (m_firstCycleVertex)
			return m_firstCycleVertex;
		if (m_processed.count(&_vertex))
			return nullptr;
		else if (m_processing.count(&_vertex))
			return m_firstCycleVertex = &_vertex;
		m_processing.insert(&_vertex);

		m_depth++;
		m_visit(_vertex, *this, m_depth);
		m_depth--;
		if (m_firstCycleVertex && m_depth == 1)
			m_firstCycleVertex = &_vertex;

		m_processing.erase(&_vertex);
		m_processed.insert(&_vertex);
		return m_firstCycleVertex;
	}

private:
	Visitor m_visit;
	std::set<V const*> m_processing;
	std::set<V const*> m_processed;
	size_t m_depth = 0;
	V const* m_firstCycleVertex = nullptr;
};

/**
 * Generic breadth first search.
 *
 * Note that V needs to be a comparable value type or a pointer.
 *
 * Example: Gather all (recursive) children in a graph starting at (and including) ``root``:
 *
 * Node const* root = ...;
 * std::set<Node const*> allNodes = BreadthFirstSearch<Node const*>{{root}}.run([](Node const* _node, auto&& _addChild) {
 *   // Potentially process ``_node``.
 *   for (Node const& _child: _node->children())
 *     // Potentially filter the children to be visited.
 *     _addChild(&_child);
 * }).visited;
 */
template<typename V>
struct BreadthFirstSearch
{
	/// Runs the breadth first search. The verticesToTraverse member of the struct needs to be initialized.
	/// @param _forEachChild is a callable of the form [...](V const& _node, auto&& _addChild) { ... }
	/// that is called for each visited node and is supposed to call _addChild(childNode) for every child
	/// node of _node.
	template<typename ForEachChild>
	BreadthFirstSearch& run(ForEachChild&& _forEachChild)
	{
		while (!verticesToTraverse.empty())
		{
			V v = std::move(verticesToTraverse.front());
			verticesToTraverse.pop_front();

			if (!visited.insert(v).second)
				continue;

			_forEachChild(v, [this](V _vertex) {
				verticesToTraverse.emplace_back(std::move(_vertex));
			});
		}
		return *this;
	}
	void abort()
	{
		verticesToTraverse.clear();
	}

	std::list<V> verticesToTraverse;
	std::set<V> visited{};
};

template<typename Graph>
concept TarjanSCCGraphConcept = requires(
	Graph const& _graph,
	typename Graph::Vertex const& _vertex
)
{
	{ _graph.edges(_vertex) } -> std::convertible_to<std::vector<typename Graph::Vertex>>;
	{ _graph.vertices() } -> std::convertible_to<std::vector<typename Graph::Vertex>>;
};
template<TarjanSCCGraphConcept Graph>
std::vector<std::vector<typename Graph::Vertex>> tarjanSCC(Graph const& _graph)
{
	std::vector<std::vector<typename Graph::Vertex>> result;

	auto const vertices = _graph.vertices();

	std::map<typename Graph::Vertex, size_t> vertexToIndex;
	for (size_t v = 0; v < _graph.vertices().size(); ++v)
		vertexToIndex[vertices[v]] = v;

	size_t index = 0;
	std::vector<std::optional<size_t>> vertexIndex(vertices.size());
	std::vector<size_t> vertexLowlink(vertices.size());
	std::vector<uint8_t> vertexOnStack(vertices.size());
	std::vector<size_t> stack;

	std::function<void(size_t)> strongConnect;
	strongConnect = [&](size_t v)
	{
		vertexIndex[v] = index;
		vertexLowlink[v] = index;
		++index;
		stack.push_back(v);
		vertexOnStack[v] = true;
		for (typename Graph::Vertex const neighborOfVertex: _graph.edges(vertices[v]))
		{
			auto const w = vertexToIndex.at(neighborOfVertex);
			if (!vertexIndex[w])
			{
				strongConnect(w);
				vertexLowlink[v] = std::min(vertexLowlink[v], vertexLowlink[w]);
			}
			else if (vertexOnStack[w])
			{
				vertexLowlink[v] = std::min(vertexLowlink[w], *vertexIndex[w]);
			}
		}

		if (vertexLowlink[v] == vertexIndex[v])
		{
			// new SCC
			result.emplace_back();
			size_t w;
			do
			{
				w = stack.back();
				stack.pop_back();
				result.back().push_back(vertices[w]);
				vertexOnStack[w] = false;
			} while (w != v);
		}
	};
	for (size_t v = 0; v < _graph.vertices().size(); ++v)
		if (!vertexIndex[v])
			strongConnect(v);

	return result;
}

}
