#include <ui/UserInterface.h>

#include <context/CodeContextDecoder.h>

#include <data/InstructionInfo.h>
#include <ui/InstructionTableView.h>
#include <ui/Styles.h>

#include <Compiler.h>

#include <libevmasm/Instruction.h>

#include <boost/algorithm/string/case_conv.hpp>

#include <fmt/format.h>

#include <iostream>

namespace ethdebug
{

InstructionTableView::InstructionTableView(
	UserInterface& _ui,
	Compiler::Ptr _compiler,
	std::string const& _filename,
	std::string const& _contract,
	std::function<void(size_t, solidity::Json const&)> const& _hover,
	std::function<void(size_t, solidity::Json const&)> const& _click)
	: m_ui(_ui), m_onHover(_hover), m_onClick(_click), m_compiler(_compiler), m_currentFilename(_filename), m_currentContract(_contract)
{
	m_decoders["code"] = CodeContextDecoder;
	m_instruction_info = solidity::Json::parse(data::InstructionInfo);
	solidity::Json instructionTypes = solidity::Json::parse(data::InstructionTypes);
	for (auto const& [type, opcodes]: instructionTypes.items())
		for (auto const& opcode: opcodes)
			m_instruction_types[opcode] = type;
	reloadInstructions();
}

void InstructionTableView::setFileContract(std::string const& _filename, std::string const& _contract)
{
	m_currentFilename = _filename;
	m_currentContract = _contract;
	reloadInstructions();
}

void InstructionTableView::setShowCreationCode(bool _enable)
{
	if (m_showCreationCode != _enable)
	{
		m_ui.setShowCreationCode(_enable);
		m_showCreationCode = _enable;
		reloadInstructions();
	}
}

void InstructionTableView::reloadInstructions()
{
	if (!m_compiler)
		return;
	m_instructions = m_compiler->instructions(m_currentFilename, m_currentContract, m_showCreationCode);
	m_hoveredInstruction = static_cast<size_t>(-1);
	m_selectedInstruction = static_cast<size_t>(-1);
	m_currentInstruction = static_cast<size_t>(-1);
}

void InstructionTableView::render()
{
	if (!m_compiler)
		return;

		bool showCreationCode = m_showCreationCode;

		if (showCreationCode)
			styles::PushButtonStyle();
		if (ImGui::Button("Creation Code"))
		{
			setShowCreationCode(true);
		}
		if (showCreationCode)
			styles::PopButtonStyle();

		ImGui::SameLine();

		if (!showCreationCode)
			styles::PushButtonStyle();
		if (ImGui::Button("Runtime Code"))
		{
			setShowCreationCode(false);
		}
		if (!showCreationCode)
			styles::PopButtonStyle();

	ImGui::BeginChild("InstructionTable", ImVec2(0, -FLT_MIN), true, ImGuiWindowFlags_HorizontalScrollbar);

	if (ImGui::BeginTable(
			"Instructions",
			5,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("##Cursor", ImGuiTableColumnFlags_WidthFixed, 16.0f * m_ui.getScale());
		ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30.0f * m_ui.getScale());
		ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthFixed, 100.0f * m_ui.getScale());
		ImGui::TableSetupColumn("Arguments", ImGuiTableColumnFlags_WidthStretch, 150.0f * m_ui.getScale());
		ImGui::TableSetupColumn("Contexts", ImGuiTableColumnFlags_WidthFixed, 60.0f * m_ui.getScale());
		ImGui::TableHeadersRow();

		auto checkHoveredOrClicked = [&](size_t _instructionIndex, solidity::Json const& _instruction)
		{
			if (ImGui::IsItemHovered())
			{
				m_hoveredInstruction = _instructionIndex;
				if (m_onHover)
					m_onHover(_instructionIndex, _instruction);
			}
			if (ImGui::IsItemClicked())
			{
				m_selectedInstruction = _instructionIndex;
				if (m_onClick)
					m_onClick(_instructionIndex, _instruction);
			}
		};

		size_t instructionIndex = 0;
		for (auto const& instruction: m_instructions)
		{
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			if (m_currentInstruction == instructionIndex)
			{
				ImGui::Text(">>");
				checkHoveredOrClicked(instructionIndex, instruction);
			}

			std::string mnemonic{};
			if (instruction["operation"].contains("mnemonic"))
				mnemonic = instruction["operation"]["mnemonic"].get<std::string>();

			std::string type;
			if (m_instruction_types.find(mnemonic) != m_instruction_types.end())
				type = m_instruction_types[mnemonic];
			else
				std::cout << "no info for mnemonic " << mnemonic << std::endl;

			ImGui::TableSetColumnIndex(1);
			if (instruction.contains("offset"))
			{
				int offset = instruction["offset"].get<int>();
				ImVec4 color{1.0f, 1.0f, 1.0f, 1.0f};
				if (mnemonic == "JUMPDEST")
					color = ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
				ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::Text("%04x", offset);
				checkHoveredOrClicked(instructionIndex, instruction);
				ImGui::PopStyleColor();
			}

			ImGui::TableSetColumnIndex(2);
			if (instruction["operation"].contains("mnemonic"))
			{
				ImVec4 color{1.0f, 1.0f, 1.0f, 1.0f};
				if (styles::InstructionTypeColors.find(type) != styles::InstructionTypeColors.end())
					color = styles::InstructionTypeColors[type];
				if (mnemonic == "JUMPDEST")
					color = ImVec4(1.0f, 0.0f, 1.0f, 1.0f);
				ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::TextUnformatted(mnemonic.c_str());
				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered())
				{
					auto const opcode = solidity::evmasm::c_instructions.find(mnemonic);
					if (opcode != solidity::evmasm::c_instructions.end())
					{
						std::stringstream opcodeId;
						opcodeId << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(opcode->second);
						solidity::Json const& info = m_instruction_info[opcodeId.str()];
						std::string input;
						if (info.contains("input"))
							input = info["input"].get<std::string>();
						std::string output;
						if (info.contains("output"))
							output = info["output"].get<std::string>();
						std::string description;
						if (info.contains("description"))
							description = info["description"].get<std::string>();
						std::string additionalInformation = mnemonic + " (0x" + opcodeId.str() + ")\n\n"
							+ (input.empty() ? "" : fmt::sprintf("INPUT:\n  %s", input + "\n"))
							+ (output.empty() ? "" : fmt::sprintf("OUTPUT:\n  %s", output + "\n"))
							+ fmt::sprintf("%s",((input.empty() && output.empty()) ? "" : "\n") + description);
						ImGui::SetTooltip("%s", additionalInformation.c_str());
					} else
					{
						// ImGui::SetTooltip("%s", fmt::sprintf("Not found in instruction database '%s'", mnemonic).c_str());
					}
				}
				checkHoveredOrClicked(instructionIndex, instruction);
			}

			if (instruction["operation"].contains("arguments"))
			{
				ImGui::TableSetColumnIndex(3);
				std::string arguments = instruction["operation"]["arguments"].begin()->get<std::string>();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
				ImGui::TextUnformatted(arguments.c_str());
				checkHoveredOrClicked(instructionIndex, instruction);
				ImGui::PopStyleColor();
			}

			ImGui::TableSetColumnIndex(4);
			{
				for (auto const& [context, contextData]: instruction["context"].items())
				{
					if (ImGui::SmallButton(boost::to_upper_copy(fmt::format("{0}##_{1}", context, instructionIndex)).c_str()))
					{
					}
					if (ImGui::IsItemHovered())
					{
						std::string additionalInformation;
						if (m_decoders.find(context) != m_decoders.end())
							additionalInformation = "\n\n" + m_decoders[context](m_compiler->ethdebug(), instruction, contextData);
						ImGui::SetTooltip("%s%s", contextData.dump(4).c_str(), additionalInformation.c_str());
					}
					checkHoveredOrClicked(instructionIndex, instruction);
				}
			}

			if (m_selectedInstruction == instructionIndex)
			{
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(66, 143, 0, 255));
			}
			else if (m_hoveredInstruction == instructionIndex)
			{
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(26, 103, 0, 255));
			}
			else if (m_currentInstruction == instructionIndex)
			{
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(46, 83, 0, 255));
			}

			++instructionIndex;
		}

		ImGui::EndTable();
	}

	ImGui::EndChild();
}

} // namespace ethdebug
