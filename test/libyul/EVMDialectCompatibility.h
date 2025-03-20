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

#include <libyul/backends/evm/EVMDialect.h>

#include <boost/test/unit_test.hpp>

#include <fmt/format.h>

#include <cstdint>
#include <optional>
#include <vector>


namespace solidity::yul::test
{

inline void requireBuiltinCompatibility(EVMDialect const& _dialect1, EVMDialect const& _dialect2, std::string_view const _builtin)
{
	if (auto const currentHandle = _dialect1.findBuiltin(_builtin))
		if (auto const handle = _dialect2.findBuiltin(_builtin))
			BOOST_REQUIRE_MESSAGE(
				*handle == *currentHandle,
				fmt::format("Failed for \"{}\" with IDs {} =/= {}.", _builtin, currentHandle->id, handle->id)
			);
}

struct EVMDialectConfigurationToTest
{
	EVMDialect const& dialect() const
	{
		return objectAccess ? EVMDialect::strictAssemblyForEVMObjects(evmVersion, eofVersion) : EVMDialect::strictAssemblyForEVM(evmVersion, eofVersion);
	}

	friend std::ostream& operator<<(std::ostream& _out, EVMDialectConfigurationToTest const& _config)
	{
		_out << fmt::format(
			"EVMConfigurationToTest[{}, eof={}, objectAccess={}]",
			_config.evmVersion.name(),
			_config.eofVersion.has_value() ? std::to_string(*_config.eofVersion) : "null",
			_config.objectAccess
		);
		return _out;
	}

	langutil::EVMVersion evmVersion;
	std::optional<uint8_t> eofVersion;
	bool objectAccess;
};

inline std::vector<EVMDialectConfigurationToTest> generateEVMDialectConfigurationsToTest(std::optional<uint8_t> _eofVersion)
{
	std::vector<EVMDialectConfigurationToTest> configs;
	for (bool providesObjectAccess: {false, true})
		for (auto evmVersion: langutil::EVMVersion::allVersions())
			if (!_eofVersion || evmVersion >= langutil::EVMVersion::firstWithEOF())
				configs.push_back(EVMDialectConfigurationToTest{evmVersion, _eofVersion, providesObjectAccess});
	return configs;
}

}
