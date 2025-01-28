#include <fmt/core.h>
#include <fmt/printf.h>

#include <ui/Styles.h>
#include <ui/UserInterface.h>

#include <imgui.h>
#include <iostream>

namespace ethdebug
{

void UserInterface::renderSelectionTabs()
{
	std::string currentSource = m_currentFilename;
	std::string currentContract = m_currentContract;
	if (m_compiler->files().size() > 1)
	{
		for (std::string const& filename: m_compiler->files())
		{
			if (filename == currentSource)
				styles::PushButtonStyle();
			if (ImGui::Button(filename.c_str()))
			{
				m_currentFilename = filename;
				m_currentContract = *m_compiler->contracts(m_currentFilename).begin();
				reload();
			}
			if (filename == currentSource)
				styles::PopButtonStyle();
			if (filename != *m_compiler->files().rbegin())
				ImGui::SameLine();
		}
		ImGui::Separator();
	}
	if (m_compiler->contracts(m_currentFilename).size() > 1 || m_compiler->files().size() > 1)
	{
		for (std::string const& contract: m_compiler->contracts(m_currentFilename))
		{
			if (contract == currentContract)
				styles::PushButtonStyle();
			if (ImGui::Button(contract.c_str()))
			{
				m_currentContract = contract;
				reload();
			}
			if (contract == currentContract)
				styles::PopButtonStyle();
			if (contract != *m_compiler->contracts(m_currentFilename).rbegin())
				ImGui::SameLine();
		}
		ImGui::Separator();
	}
	m_currentArtifact = m_compiler->artifact(m_currentFilename, m_currentContract);
}

void UserInterface::setShowCreationCode(bool _enable)
{
	m_showCreationCode = _enable;
}

void UserInterface::reload()
{
	m_currentArtifact = m_compiler->artifact(m_currentFilename, m_currentContract);

	if (!m_ethdebugGlobal)
		m_ethdebugGlobal = std::make_shared<HighlightedTextView>(m_compiler->ethdebug().dump(4));
	if (!m_ethdebugCreation)
		m_ethdebugCreation = std::make_shared<HighlightedTextView>(m_currentArtifact->ethdebugCreation.dump(4));
	else
		m_ethdebugCreation->setText(m_currentArtifact->ethdebugCreation.dump(4));
	if (!m_ethdebugRuntime)
		m_ethdebugRuntime = std::make_shared<HighlightedTextView>(m_currentArtifact->ethdebugRuntime.dump(4));
	else
		m_ethdebugRuntime->setText(m_currentArtifact->ethdebugRuntime.dump(4));

	if (!m_soliditySourceView)
	{
		m_soliditySourceView = std::make_shared<HighlightedTextView>(m_currentArtifact->solidityCode);
		m_soliditySourceView->addHighlightRegion(HighlightRegion{IM_COL32(50, 250, 50, 128), 0, 0});
		m_soliditySourceView->addHighlightRegion(HighlightRegion{IM_COL32(250, 50, 50, 128), 0, 0});
	}
	else
	{
		m_soliditySourceView->setText(m_currentArtifact->solidityCode);
		m_soliditySourceView->updateHighlightRegion(0, 0, 0);
		m_soliditySourceView->updateHighlightRegion(1, 0, 0);
	}

	if (!m_yulSourceView)
	{
		m_yulSourceView = std::make_shared<HighlightedTextView>(m_currentArtifact->yulCode);
		m_yulSourceView->addHighlightRegion(HighlightRegion{IM_COL32(20, 100, 20, 128), 0, 0});
		m_yulSourceView->addHighlightRegion(HighlightRegion{IM_COL32(100, 20, 20, 128), 0, 0});
	}
	else
	{
		m_yulSourceView->setText(m_currentArtifact->yulCode);
		m_yulSourceView->updateHighlightRegion(0, 0, 0);
		m_yulSourceView->updateHighlightRegion(1, 0, 0);
	}

	if (!m_instructionTableView)
	{
		m_instructionTableView
			= std::make_shared<InstructionTableView>(*this, m_compiler, m_currentFilename, m_currentContract);
		m_instructionTableView->setOnClick(
			[&](size_t, solidity::Json const& _instruction)
			{
				size_t start = _instruction["context"]["code"]["range"]["offset"].get<size_t>();
				size_t end = start + _instruction["context"]["code"]["range"]["length"].get<size_t>();
				m_soliditySourceView->updateHighlightRegion(0, start, end);
			});
		m_instructionTableView->setOnHover(
			[&](size_t, solidity::Json const& _instruction)
			{
				size_t start = _instruction["context"]["code"]["range"]["offset"].get<size_t>();
				size_t end = start + _instruction["context"]["code"]["range"]["length"].get<size_t>();
				m_soliditySourceView->updateHighlightRegion(1, start, end);
			});
	}
	else
	{
		m_instructionTableView->setFileContract(m_currentFilename, m_currentContract);
		m_instructionTableView->reloadInstructions();
	}
}

void UserInterface::render()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
									| ImGuiWindowFlags_NoSavedSettings;

	ImGui::Begin("ethdebug tool", nullptr, window_flags);

	renderSelectionTabs();

	ImGui::Columns(2, nullptr, false);
	// left side of window
	m_instructionTableView->render();

	// right side of window
	ImGui::NextColumn();

	std::string activeTab = m_currentTab;

	if (activeTab == "Raw Debug Data")
		styles::PushButtonStyle();
	if (ImGui::Button("Raw Debug Data"))
		m_currentTab = "Raw Debug Data";
	if (activeTab == "Raw Debug Data")
		styles::PopButtonStyle();

	ImGui::SameLine();

	if (activeTab == "Sources")
		styles::PushButtonStyle();
	if (ImGui::Button("Sources"))
		m_currentTab = "Sources";
	if (activeTab == "Sources")
		styles::PopButtonStyle();

	if (activeTab == "Raw Debug Data")
	{
		static float topPanelHeight = 100 * m_scale;
		ImGui::BeginChild("Global", ImVec2(ImGui::GetContentRegionAvail().x, topPanelHeight), true);
		m_ethdebugGlobal->render();
		ImGui::EndChild();

		ImVec4 windowBgColor = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
		ImGui::PushStyleColor(ImGuiCol_Header, windowBgColor);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, windowBgColor);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, windowBgColor);
		ImGui::Selectable("##separator-ethdebug", false, ImGuiSelectableFlags_Highlight);
		ImGui::PopStyleColor(3);

		if (ImGui::IsItemActive() || ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			topPanelHeight += ImGui::GetIO().MouseDelta.y;

		ImGui::BeginChild(m_showCreationCode ? "Creation Code" : "Runtime Code", ImVec2(ImGui::GetContentRegionAvail().x, 0), true);
		if (m_showCreationCode)
			m_ethdebugCreation->render();
		else
			m_ethdebugRuntime->render();
		ImGui::EndChild();
	}

	if (activeTab == "Sources")
	{
		static float topPanelHeight = ImGui::GetContentRegionAvail().y / 2;

		ImGui::BeginChild("SoliditySource", ImVec2(ImGui::GetContentRegionAvail().x, topPanelHeight), true);
		m_soliditySourceView->render();
		ImGui::EndChild();

		ImVec4 windowBgColor = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
		ImGui::PushStyleColor(ImGuiCol_Header, windowBgColor);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, windowBgColor);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, windowBgColor);
		ImGui::Selectable("##separator-source", false, ImGuiSelectableFlags_Highlight);
		ImGui::PopStyleColor(3);

		if (ImGui::IsItemActive() || ImGui::IsItemHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			topPanelHeight += ImGui::GetIO().MouseDelta.y;

		ImGui::BeginChild("YulSource", ImVec2(ImGui::GetContentRegionAvail().x, 0), true);
		m_yulSourceView->render();
		ImGui::EndChild();
	}

	ImGui::End();
}
}
