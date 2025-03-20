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

#include <test/libyul/EVMDialectCompatibility.h>

#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

namespace bdata = boost::unit_test::data;

using namespace solidity;
using namespace solidity::yul;
using namespace solidity::yul::test;

BOOST_AUTO_TEST_SUITE(EVMDialectCompatibility)

BOOST_DATA_TEST_CASE(
	builtin_function_handle_compatibility_non_eof,
	bdata::make(generateEVMDialectConfigurationsToTest(std::nullopt)),
	evmDialectConfigurationToTest
)
{
	auto const& dialectToTestAgainst = evmDialectConfigurationToTest.dialect();
	// no object access for current
	{
		auto const& currentDialect = EVMDialect::strictAssemblyForEVM({}, std::nullopt);
		for (auto const& builtinFunctionName: currentDialect.builtinFunctionNames())
			requireBuiltinCompatibility(currentDialect, dialectToTestAgainst, builtinFunctionName);
	}
	// object access for current
	{
		auto const& currentDialect = EVMDialect::strictAssemblyForEVMObjects({}, std::nullopt);
		for (auto const& builtinFunctionName: currentDialect.builtinFunctionNames())
			requireBuiltinCompatibility(currentDialect, dialectToTestAgainst, builtinFunctionName);
	}
}

BOOST_DATA_TEST_CASE(
	builtin_function_handle_compatibility_eof,
	bdata::monomorphic::grid(
		bdata::make(generateEVMDialectConfigurationsToTest(std::nullopt)) + bdata::make(generateEVMDialectConfigurationsToTest(1)),
		bdata::make({false, true})
	),
	evmDialectConfigurationToTest,
	withEOF
)
{
	auto const& dialectToTestAgainst = evmDialectConfigurationToTest.dialect();
	langutil::EVMVersion latestEVMVersion = langutil::EVMVersion::allVersions().back();
	std::optional<uint8_t> eofVersion = std::nullopt;
	if (withEOF)
		eofVersion = 1;
	// no object access for latest
	{
		auto const& latestDialect = EVMDialect::strictAssemblyForEVM(latestEVMVersion, eofVersion);
		for (auto const& builtinFunctionName: latestDialect.builtinFunctionNames())
			requireBuiltinCompatibility(latestDialect, dialectToTestAgainst, builtinFunctionName);
	}
	// object access for latest
	{
		auto const& latestDialect = EVMDialect::strictAssemblyForEVMObjects(latestEVMVersion, eofVersion);
		for (auto const& builtinFunctionName: latestDialect.builtinFunctionNames())
			requireBuiltinCompatibility(latestDialect, dialectToTestAgainst, builtinFunctionName);
	}
}

BOOST_AUTO_TEST_SUITE_END()
