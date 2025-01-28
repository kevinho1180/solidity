#pragma once

#include <fmt/printf.h>
#include <libsolutil/JSON.h>

namespace ethdebug
{

inline std::string CodeContextDecoder(solidity::Json const& _ethdebug, solidity::Json const&, solidity::Json const& _context)
{
	if (_ethdebug.is_object() && _ethdebug.contains("sources") && _ethdebug["sources"].is_array() && _context.is_object() && _context.contains("source") && _context["source"].contains("id"))
		if (_context["source"]["id"].get<int>() >= 0 && _context["source"]["id"].get<uint64_t>() < _ethdebug["sources"].size())
			return fmt::sprintf("Source: %s", (_ethdebug["sources"][(_context["source"]["id"].get<uint64_t>())]).get<std::string>());
	return {};
}


} // namespace ethdebug
