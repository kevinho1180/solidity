#pragma once

#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

namespace ethdebug
{

struct HighlightRegion
{
	ImU32 color;
	size_t start;
	size_t end;

	std::function<void(size_t, HighlightRegion const&)> onHover;
	std::function<void(size_t, HighlightRegion const&)> onClick;

	explicit HighlightRegion(
		ImU32 _color,
		size_t _start,
		size_t _end,
		std::function<void(size_t, HighlightRegion const&)> _hover = nullptr,
		std::function<void(size_t, HighlightRegion const&)> _click = nullptr)
		: color(_color), start(_start), end(_end), onHover(std::move(_hover)), onClick(std::move(_click))
	{
	}
};

class HighlightedTextView
{
public:
	typedef std::shared_ptr<HighlightedTextView> Ptr;

	explicit HighlightedTextView(std::string _text, std::vector<HighlightRegion> _regions = {});

	size_t addHighlightRegion(HighlightRegion const& _region);

	void removeHighlightRegion(size_t _regionIndex);

	void updateHighlightRegion(size_t _regionIndex, size_t _start, size_t _end);

	void clearHighlightRegions();

	[[nodiscard]] std::vector<HighlightRegion> const& getHighlightRegions() const;

	void setText(std::string const& _newText);

	[[nodiscard]] std::string const& getText() const;

	void render() const;

private:
	std::string m_text;
	std::vector<HighlightRegion> m_highlightRegions;
};

} // namespace ethdebug
