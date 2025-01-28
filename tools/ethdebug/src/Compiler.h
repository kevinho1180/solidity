#pragma once

#include <libsolutil/JSON.h>
#include <range/v3/range.hpp>
#include <range/v3/view/map.hpp>
#include <set>
#include <string>

namespace ethdebug
{

struct Artifacts
{
	typedef std::shared_ptr<Artifacts> Ptr;

	std::string name;
	std::string solidityCode;
	std::string yulCode;
	solidity::Json ethdebugCreation;
	solidity::Json ethdebugRuntime;
};

class Compiler
{
public:
	typedef std::shared_ptr<Compiler> Ptr;
	Compiler(std::set<std::string> const& _sources, bool _optimize);

	std::vector<std::string> files()
	{
		return m_contracts | ranges::views::keys | ranges::to<std::vector<std::string>>();
	}

	std::vector<std::string> contracts(std::string const& _filename)
	{
		return m_contracts[_filename] | ranges::views::keys | ranges::to<std::vector<std::string>>();
	}

	Artifacts::Ptr artifact(std::string const& _filename, std::string const& _contractName);

	solidity::Json const& instructions(std::string const& _filename, std::string const& _contractName, bool _creationCode);

	solidity::Json const& instruction(std::string const& _filename, std::string const& _contractName, bool _creationCode, size_t _instructionIndex);

	solidity::Json const& ethdebug() const { return m_ethdebug; }

private:
	void parseArtifacts();

	std::set<std::string> m_files;
	bool m_optimize;
	std::map<std::string, std::string> m_content;
	solidity::Json m_ethdebug;
	std::map<std::string, std::map<std::string, Artifacts::Ptr>> m_contracts;
	solidity::Json m_standardJson;
};

} // ethdebug
