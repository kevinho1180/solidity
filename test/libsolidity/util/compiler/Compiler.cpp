#include <test/libsolidity/util/compiler/Compiler.h>

using namespace solidity;
using namespace solidity::frontend::test;

std::optional<CompiledContract> CompilerOutput::contract(
    ContractName const& _name
) const
{
    std::string sourceName = _name.source();
    std::vector<CompiledContract> contracts;

    if (auto source = m_sourceUnits.find(sourceName); source != m_sourceUnits.end())
        contracts = source->second;

    if (contracts.empty())
        return std::nullopt;

    // Return specific contract if name was set, or last in the list.
    if (!_name.contract().empty())
    {
        std::string contractName = _name.contract();
        for (auto const& contract: contracts)
            if (
                contractName == contract.name ||
                sourceName + ":" + contractName == contract.name
            )
                return std::make_optional(contract);
        return std::nullopt;
    }
    else
    {
        auto contract = contracts.back();
        return std::make_optional(contract);
    }
}

std::optional<AnnotatedEventSignature> CompilerOutput::matchEvent(
    util::h256 const& _hash
) const
{
    for (auto const& [name, contracts]: m_sourceUnits)
        for (auto const& contract: contracts)
            for (auto const& event: contract.eventSignatures)
                if (!event.isAnonymous && keccak256(event.signature) == _hash)
                    return std::make_optional(event);

    return std::nullopt;
}

bool CompilerOutput::success() const
{
    return m_success;
}

std::optional<langutil::Error> CompilerOutput::findError(
    langutil::Error::Type _type
) const
{
    for (auto const& error: m_errors)
		if (error->type() == _type)
			return std::make_optional(*error);

    return std::nullopt;
}

std::string CompilerOutput::errorInformation() const
{
    return m_errorInformation;
}
