#pragma once

#include <ui/HighlightedTextView.h>
#include <ui/InstructionTableView.h>


#include <Compiler.h>

namespace ethdebug
{

class UserInterface
{
public:
	explicit UserInterface(Compiler::Ptr _compiler, float _scaling): m_compiler(std::move(_compiler))
	{
		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(_scaling);
		ImGuiIO& io = ImGui::GetIO();
		io.FontGlobalScale = _scaling;
		m_currentFilename = *m_compiler->files().begin();
		m_currentContract = *m_compiler->contracts(m_currentFilename).begin();
		m_showCreationCode = true;
		m_scale = _scaling;
		m_currentTab = "Raw Debug Data";
		reload();
	}

	void setShowCreationCode(bool _enable);

	void reload();

	void render();

	float getScale() const { return m_scale; }

private:
	void renderSelectionTabs();

	InstructionTableView::Ptr m_instructionTableView;
	HighlightedTextView::Ptr m_soliditySourceView;
	HighlightedTextView::Ptr m_yulSourceView;
	HighlightedTextView::Ptr m_ethdebugGlobal;
	HighlightedTextView::Ptr m_ethdebugCreation;
	HighlightedTextView::Ptr m_ethdebugRuntime;

	Compiler::Ptr m_compiler;
	std::string m_currentFilename;
	std::string m_currentContract;
	bool m_showCreationCode;
	Artifacts::Ptr m_currentArtifact;
	float m_scale;

	std::string m_currentTab;
};

} // namespace ethdebug
