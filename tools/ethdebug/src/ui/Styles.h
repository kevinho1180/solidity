#pragma once

#include <imgui.h>

#include <string>
#include <map>

namespace ethdebug::styles
{

static std::map<std::string,  ImVec4> InstructionTypeColors =
{
	{"arithmetic", ImVec4(0.0f, 1.0f, 1.0f, 1.0f)},
	{"logic",      ImVec4(1.0f, 1.0f, 0.0f, 1.0f)},
	{"stack",      ImVec4(0.0f, 1.0f, 0.0f, 1.0f)},
	{"control",    ImVec4(1.0f, 0.0f, 1.0f, 1.0f)},
	{"memory",     ImVec4(1.0f, 0.0f, 0.0f, 1.0f)},
	{"storage",    ImVec4(1.0f, 0.5f, 1.0f, 1.0f)},
	{"tstorage",    ImVec4(0.5f, 1.0f, 1.0f, 1.0f)}
};

inline void PushButtonStyle()
{
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.6f, 0.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.8f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.7f, 0.0f, 1.0f));
}

inline void PushInstructionStyle()
{
	ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(100, 180, 100, 255));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(120, 200, 120, 255));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(100, 180, 100, 255));
}

inline void PopButtonStyle() { ImGui::PopStyleColor(3); }

} // namespace ethdebug::styles
