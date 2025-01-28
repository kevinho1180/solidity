#pragma once

#include <Compiler.h>

#include <cstddef>
#include <string>
#include <vector>

namespace ethdebug
{

class UserInterface;

class InstructionTableView
{
public:
	typedef std::shared_ptr<InstructionTableView> Ptr;

	explicit InstructionTableView(
		UserInterface& _ui,
		Compiler::Ptr _compiler,
		std::string const& _filename,
		std::string const& _contract,
		std::function<void(size_t, solidity::Json const&)> const& _hover = nullptr,
		std::function<void(size_t, solidity::Json const&)> const& _click = nullptr);

	void setFileContract(std::string const& _filename, std::string const & _contract);

	void reloadInstructions();

	void render();

	void setCurrentInstruction(size_t index) { m_currentInstruction = index; }

	size_t getCurrentInstruction() const { return m_currentInstruction; }

	void setShowCreationCode(bool _enable);

	bool isShowingCreationCode() const { return m_showCreationCode; }

	size_t getSelectedInstruction() const { return m_selectedInstruction; }

	size_t getHoveredInstruction() const { return m_hoveredInstruction; }

	void setOnHover(std::function<void(size_t, solidity::Json const&)> const& _onHover) { m_onHover = _onHover; }

	void setOnClick(std::function<void(size_t, solidity::Json const&)> const& _onClick) { m_onClick = _onClick; }

private:
	UserInterface& m_ui;

	std::function<void(size_t, solidity::Json const&)> m_onHover;
	std::function<void(size_t, solidity::Json const&)> m_onClick;

	std::map<std::string, std::function<std::string(solidity::Json const&, solidity::Json const&, solidity::Json const&)>> m_decoders;

	Compiler::Ptr m_compiler;
	std::string m_currentFilename;
	std::string m_currentContract;

	bool m_showCreationCode = true;

	size_t m_hoveredInstruction = static_cast<size_t>(-1);
	size_t m_selectedInstruction = static_cast<size_t>(-1);
	size_t m_currentInstruction = static_cast<size_t>(-1);

	std::vector<nlohmann::json> m_instructions;
	nlohmann::json m_instruction_info;
	std::map<std::string, std::string> m_instruction_types;
};

} // namespace ethdebug
