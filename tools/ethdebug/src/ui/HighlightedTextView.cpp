#include <ui/HighlightedTextView.h>

#include <fmt/printf.h>

namespace ethdebug
{

HighlightedTextView::HighlightedTextView(std::string _text, std::vector<HighlightRegion> _regions)
	: m_text(std::move(_text)), m_highlightRegions(std::move(_regions))
{
}

size_t HighlightedTextView::addHighlightRegion(HighlightRegion const& _region)
{
	m_highlightRegions.push_back(_region);
	return m_highlightRegions.size() - 1;
}

void HighlightedTextView::removeHighlightRegion(size_t _regionIndex)
{
	if (_regionIndex < m_highlightRegions.size())
	{
		m_highlightRegions.erase(m_highlightRegions.begin() + (int) _regionIndex);
	}
}

void HighlightedTextView::updateHighlightRegion(size_t _regionIndex, size_t _start, size_t _end)
{
	if (_regionIndex < m_highlightRegions.size())
	{
		m_highlightRegions[_regionIndex].start = _start;
		m_highlightRegions[_regionIndex].end = _end;
	}
}

void HighlightedTextView::clearHighlightRegions() { m_highlightRegions.clear(); }

std::vector<HighlightRegion> const& HighlightedTextView::getHighlightRegions() const { return m_highlightRegions; }

void HighlightedTextView::setText(std::string const& _newText) { m_text = _newText; }

std::string const& HighlightedTextView::getText() const { return m_text; }

void HighlightedTextView::render() const
{
	if (m_text.empty())
		return;

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 text_pos = ImGui::GetCursorScreenPos();
	static float font_size = ImGui::GetFontSize();
	static float char_width = ImGui::CalcTextSize("A").x;

	ImGui::TextUnformatted(m_text.c_str());

	ImGui::SetCursorScreenPos(text_pos);

	std::vector<std::string> lines;
	lines.reserve(64);

	{
		std::string current_line;
		for (char const* p = m_text.c_str(); *p; ++p)
		{
			if (*p == '\n')
			{
				lines.push_back(current_line);
				current_line.clear();
			}
			else
			{
				current_line.push_back(*p);
			}
		}
		if (!current_line.empty())
			lines.push_back(current_line);
	}

	size_t global_char_index = 0;
	float y_offset = 0.0f;

	for (size_t line_idx = 0; line_idx < lines.size(); line_idx++)
	{
		std::string const& line = lines[line_idx];
		size_t line_start_idx = global_char_index;
		size_t line_end_idx = line_start_idx + line.size();

		for (size_t region_idx = 0; region_idx < m_highlightRegions.size(); region_idx++)
		{
			HighlightRegion const& region = m_highlightRegions[region_idx];

			if (region.start >= region.end)
				continue;

			if (region.end <= line_start_idx || region.start >= line_end_idx)
				continue;

			size_t highlight_start = std::max(region.start, line_start_idx);
			size_t highlight_end = std::min(region.end, line_end_idx);

			size_t relative_start = highlight_start - line_start_idx;
			size_t relative_end = highlight_end - line_start_idx;

			ImVec2 highlight_rect_min(
				text_pos.x + (static_cast<float>(relative_start) * char_width), text_pos.y + y_offset);
			ImVec2 highlight_rect_max(
				text_pos.x + (static_cast<float>(relative_end) * char_width), text_pos.y + y_offset + font_size);

			draw_list->AddRectFilled(highlight_rect_min, highlight_rect_max, region.color);

			ImGui::PushID(static_cast<int>(region_idx));
			ImGui::SetCursorScreenPos(highlight_rect_min);

			ImVec2 button_size(
				(static_cast<float>(relative_end) - static_cast<float>(relative_start)) * char_width + 1, font_size);
			ImGui::InvisibleButton(fmt::sprintf("##HighlightRegion%d%d", line_idx, region_idx).c_str(), button_size);

			if (ImGui::IsItemHovered() && region.onHover)
				region.onHover(region_idx, region);
			if (ImGui::IsItemClicked() && region.onClick)
				region.onClick(region_idx, region);

			ImGui::PopID();

			ImGui::SetCursorScreenPos(highlight_rect_min);
			ImGui::TextUnformatted(line.substr(relative_start, relative_end - relative_start).c_str());

			ImGui::SetCursorScreenPos(ImVec2(text_pos.x, text_pos.y + y_offset));
		}

		y_offset += font_size;
		global_char_index += line.size();
		if (line_idx < lines.size() - 1)
			global_char_index += 1;
	}
}

} // namespace ethdebug
