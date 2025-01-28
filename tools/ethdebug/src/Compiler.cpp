#include <Compiler.h>

#include <libsolidity/interface/StandardCompiler.h>
#include <libsolutil/JSON.h>

#include <fstream>

namespace ethdebug
{

Compiler::Compiler(std::set<std::string> const& _sources, bool _optimize): m_files(_sources), m_optimize(_optimize)
{
	using namespace solidity::frontend;
	solidity::frontend::StandardCompiler compiler;

	solidity::Json input;
	input["language"] = "Solidity";
	(void) m_optimize;

	solidity::Json sources;
	for (auto const& file: _sources)
	{
		std::ifstream ifs(file);
		if (!ifs.is_open())
		{
			std::cerr << "Could not open: " << file << "\n";
			continue;
		}
		std::string sourceContent((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
		sources[file]["content"] = sourceContent;
		m_content[file] = sourceContent;
	}
	input["sources"] = sources;

	solidity::Json settings;
	solidity::Json outputSel;
	outputSel["*"]["*"] = nlohmann::json::array({"evm.bytecode.ethdebug", "evm.deployedBytecode.ethdebug", "ir"});
	settings["outputSelection"] = outputSel;
	settings["viaIR"] = true;
	if (_optimize)
	{
		nlohmann::json optimizer;
		optimizer["enabled"] = true;
		settings["optimizer"] = optimizer;
	}
	input["settings"] = settings;

	m_standardJson = compiler.compile(input);

	bool errors = false;
	if (m_standardJson.contains("errors"))
		for (auto const& error: m_standardJson["errors"])
			if (error["severity"] == "error")
			{
				std::cout << "Error: " << error["formattedMessage"].get<std::string>() << std::endl;
				errors = true;
			}
	if (errors)
		exit(1);

	parseArtifacts();
}

void Compiler::parseArtifacts()
{
	m_ethdebug = m_standardJson["ethdebug"];
	for (auto const& [fileName, contracts]: m_standardJson["contracts"].items())
	{
		for (auto const& [contractName, data]: contracts.items())
		{
			std::string irContent = m_standardJson["contracts"][fileName][contractName]["ir"].get<std::string>();
			if (!irContent.empty())
			{
				Artifacts::Ptr artifact = std::make_shared<Artifacts>();
				artifact->name = contractName;
				artifact->ethdebugCreation = data["evm"]["bytecode"]["ethdebug"];
				artifact->ethdebugRuntime = data["evm"]["deployedBytecode"]["ethdebug"];
				artifact->yulCode = irContent;
				artifact->solidityCode = m_content[fileName];
				m_contracts[fileName][contractName] = artifact;
			}
		}
	}
}

Artifacts::Ptr Compiler::artifact(std::string const& _filename, std::string const& _contractName)
{
	if (m_contracts.count(_filename) && m_contracts[_filename].count(_contractName))
		return m_contracts[_filename][_contractName];
	return {};
}

solidity::Json const&
Compiler::instructions(std::string const& _filename, std::string const& _contractName, bool _creationCode)
{
	if (m_contracts.count(_filename) && m_contracts[_filename][_contractName])
		return _creationCode
			? m_contracts[_filename][_contractName]->ethdebugCreation["instructions"]
			: m_contracts[_filename][_contractName]->ethdebugRuntime["instructions"];

	static solidity::Json empty = solidity::Json::array();
	return empty;
}

solidity::Json const& Compiler::instruction(std::string const& _filename, std::string const& _contractName, bool _creationCode, size_t _instructionIndex)
{
	solidity::Json const& instructions = this->instructions(_filename, _contractName, _creationCode);
	solAssert(instructions.is_array());
	if (_instructionIndex >= 0 && _instructionIndex < instructions.size())
	{
		return instructions[_instructionIndex];
	}
	static solidity::Json empty = solidity::Json::object();
	return empty;
}

} // ethdebug
