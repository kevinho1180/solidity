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

#include <libevmasm/Assembly.h>

#include <libsolidity/interface/CompilerStack.h>

#include <libsolutil/Common.h>

using namespace solidity;
using namespace solidity::util;

namespace solidity::frontend::test
{

struct AnnotatedEventSignature
{
	std::string signature;
	std::vector<std::string> indexedTypes;
	std::vector<std::string> nonIndexedTypes;
	bool isAnonymous;
};

enum class MetadataFormat {
	WithReleaseVersionTag,
	WithPrereleaseVersionTag,
	NoMetadata
};

enum class MetadataHash {
	IPFS,
	Bzzr1,
	None
};

/**
 * Helper for contract lookups. A `ContractName` can be initialized from
 *  either an unqualified contract name like "C" or a qualified name
 * like ":C" or "lib.sol:C".
 */
class ContractName
{
public:
	/// Construct an empty, unqualified contract name
	ContractName(): m_isUnqualified(true) {}

	/// Construct a new contract name by parsing the name given.
	/// @param _name the contract name, e.g. "C", ":C" or "lib.sol:C"
	ContractName(std::string const& _name)
	{
		auto pos = _name.find(":");
		if (pos != std::string::npos)
		{
			m_isUnqualified = false;
			m_source = _name.substr(0, pos);
			m_contract = _name.substr(pos + 1);
		}
		else
		{
			m_isUnqualified = true;
			m_contract = _name;
		}
	}

	/// Support implicit conversion from string literals.
	ContractName(const char* _name): ContractName(std::string{_name}) {}

	/// Construct a fully-qualified contract name.
	/// @param _source the source name
	/// @param _contract the contract name
	ContractName(std::string const& _source, std::string const& _contract):
		m_source(_source),
		m_contract(_contract),
		m_isUnqualified(false)
	{}

	/// @returns the source name.
	std::string const& source() const
	{
		return m_source;
	}

	/// @returns the contractname.
	std::string const& contract() const
	{
		return m_contract;
	}

	/// @returns false if this was initialized from an unqualified contract
	/// name like "C".
	bool isUnqualified() const
	{
		return m_isUnqualified;
	}

private:
	/// The source name. Can be empty.
	std::string m_source;
	/// The contract name. Can be empty.
	std::string m_contract;
	/// True, if this is an unqualified contract name.
	bool m_isUnqualified;
};

struct CompilerInput
{
	CompilerInput() = default;

	/// Source code to be compiled
	std::map<std::string, std::string> sourceCode;
	/// Information on which library is deployed where
	std::map<std::string, solidity::util::h160> libraryAddresses;
	/// Contract name without a colon prefix
	std::optional<std::string> contractName;
	/// EVM target version
	std::optional<langutil::EVMVersion> evmVersion;
	/// EOF version
	std::optional<uint8_t> eofVersion;
	/// If optimisation should be enabled.
	std::optional<bool> optimise;
	/// Optimiser setting to be used during compilation
	std::optional<frontend::OptimiserSettings> optimiserSettings;
	/// Revert string behaviour
	std::optional<RevertStrings> revertStrings;
	/// Format of the metadata appended at the end of the bytecode
	std::optional<MetadataFormat> metadataFormat;
	/// Hash should be used to store the metadata in the bytecode
	std::optional<MetadataHash> metadataHash;
	/// Flag used for debugging
	std::optional<bool> debugFailure;
	/// Flag to enable new code generator
	std::optional<bool> viaIR;
};

/**
 * A compiled contract.
 */
struct CompiledContract
{
	/// The name of the contract
	std::string name;
	/// Bytecode of the assembled object for a contract.
	bytes object;
	/// Bytecode of the assembled runtime object for a contract.
	bytes runtimeObject;
	/// If object has unlinked references.
	bool hasUnlinkedReferences;
	/// CBOR-encoded metadata matching the pipeline selected using the viaIR
	/// setting.
	bytes cborMetadata;
	/// Normal contract assembly items.
	std::optional<evmasm::AssemblyItems> assemblyItems;
	/// Normal contract runtime assembly items.
	std::optional<evmasm::AssemblyItems> runtimeAssemblyItems;
	/// Contract Metadata matching the pipeline selected using the viaIR setting.
	std::optional<std::string> metadata;
	/// The contract ABI as a JSON object.
	std::optional<Json> contractABI;
	/// JSON object with the three members ``methods``, ``events``, ``errors``.
	/// Each is a map, mapping identifiers (hashes) to function names.
	std::optional<Json> interfaceSymbols;
	/// Event signatures
	std::vector<AnnotatedEventSignature> eventSignatures;
};

using SourceUnits = std::map<std::string, std::vector<CompiledContract>>;

/**
 * Output generated by the compiler. Provides convenient access method to the
 * internal data.
 */
class CompilerOutput
{
public:
	CompilerOutput(
		SourceUnits _sourceUnits,
		bool _success,
		langutil::ErrorList _errors,
		std::string _errorInformation
	):
		m_sourceUnits(_sourceUnits),
		m_success(_success),
		m_errors(_errors),
		m_errorInformation(_errorInformation)
	{}

	/// @returns the compiled contract, if any was found.
	/// @param _name find specific contract, if set. Default: return last contract
	/// from source with empty name.
	std::optional<CompiledContract> contract(ContractName const& _name = {}) const;

	/// @returns the annotated event sig for the hash given, if any.
	/// @param hash
	std::optional<AnnotatedEventSignature> matchEvent(
		util::h256 const& _hash
	) const;

	/// @returns true if no errors occurred during compilation.
	bool success() const;

	/// @returns the first error that matches the type specified, or none if no
	/// error for this type was found.
	/// @param _type the error type to look for.
	std::optional<langutil::Error> findError(langutil::Error::Type _type) const;

	/// @returns a formatted output of all errors that occurred during
	/// compilation.
	std::string errorInformation() const;

private:
	/// All compiled contracts, indexed by source name.
	SourceUnits m_sourceUnits;
	/// If compilation was successful.
	bool m_success;
	/// All errors, if any.
	langutil::ErrorList m_errors;
	/// Formatted error information.
	std::string m_errorInformation;
};

}
