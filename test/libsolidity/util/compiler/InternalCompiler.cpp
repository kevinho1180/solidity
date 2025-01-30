#include <test/libsolidity/util/compiler/InternalCompiler.h>

using namespace solidity;
using namespace solidity::evmasm;
using namespace solidity::frontend::test;

CompilerOutput InternalCompiler::compile(CompilerInput const& _input)
{
    configure(_input);

    auto [contracts, errors] = internalCompile(_input);
    bool success = m_stack.compilationSuccessful();

    return CompilerOutput{
        contracts,
        success,
        errors,
        formatErrorInformation()
    };
}

void InternalCompiler::configure(CompilerInput const& _input)
{
    m_stack.reset();

    m_stack.setSources(_input.sourceCode);
    m_stack.setLibraries(_input.libraryAddresses);

    if (_input.evmVersion.has_value())
        m_stack.setEVMVersion(_input.evmVersion.value());
    if (_input.optimise.has_value())
        m_stack.setOptimiserSettings(_input.optimise.value());
    if (_input.optimiserSettings.has_value())
        m_stack.setOptimiserSettings(_input.optimiserSettings.value());
    if (_input.revertStrings.has_value())
        m_stack.setRevertStringBehaviour(_input.revertStrings.value());
    if (_input.metadataFormat.has_value())
    {
        CompilerStack::MetadataFormat metadata = CompilerStack::MetadataFormat::NoMetadata;
        switch (_input.metadataFormat.value())
        {
            case MetadataFormat::WithReleaseVersionTag:
                metadata = CompilerStack::MetadataFormat::WithReleaseVersionTag;
                break;
            case MetadataFormat::WithPrereleaseVersionTag:
                metadata = CompilerStack::MetadataFormat::WithPrereleaseVersionTag;
                break;
            default:
                break;
        }
        m_stack.setMetadataFormat(metadata);
    }
    if (_input.metadataHash.has_value())
    {
        CompilerStack::MetadataHash hash = CompilerStack::MetadataHash::None;
        switch (_input.metadataHash.value())
        {
            case MetadataHash::IPFS:
                hash = CompilerStack::MetadataHash::IPFS;
                break;
            case MetadataHash::Bzzr1:
                hash = CompilerStack::MetadataHash::Bzzr1;
                break;
            default:
                break;
        }
        m_stack.setMetadataHash(hash);
    }
    if (_input.viaIR.has_value())
        m_stack.setViaIR(_input.viaIR.value());
    m_stack.setEOFVersion(_input.eofVersion);
}

std::pair<MappedContracts, Errors> InternalCompiler::internalCompile(
    CompilerInput const& _input
)
{
    m_stack.compile();

    MappedContracts mappedContracts{};
    for (auto sourceName: m_stack.sourceNames())
    {
        std::vector<CompiledContract> compiled;
        for (auto contract: m_stack.contractDefinitions(sourceName))
        {
            std::string contractName = contract->fullyQualifiedName();
            LinkerObject object = m_stack.object(contractName);
            LinkerObject runtimeObject = m_stack.runtimeObject(contractName);
            bool hasUnlinkedReferences = !object.linkReferences.empty();
            bytes cborMetadata = m_stack.cborMetadata(contractName);

            std::optional<AssemblyItems> assemblyItems;
            std::optional<AssemblyItems> runtimeAssemblyItems;

            if (!_input.eofVersion.has_value())
            {
                if (auto items = m_stack.assemblyItems(contractName); items != nullptr)
                    assemblyItems = std::make_optional(*items);

                if (auto items = m_stack.runtimeAssemblyItems(contractName); items != nullptr)
                    runtimeAssemblyItems = std::make_optional(*items);
            }

            if (!m_stack.isExperimentalSolidity())
                compiled.emplace_back(CompiledContract{
                    contractName,
                    object.bytecode,
                    runtimeObject.bytecode,
                    hasUnlinkedReferences,
                    cborMetadata,
                    assemblyItems,
                    runtimeAssemblyItems,
                    m_stack.metadata(contractName),
                    m_stack.contractABI(contractName),
                    m_stack.interfaceSymbols(contractName),
                    generateEventSignatures(contractName)
                });
            else
                compiled.emplace_back(CompiledContract{
                    contractName,
                    object.bytecode,
                    runtimeObject.bytecode,
                    hasUnlinkedReferences,
                    cborMetadata,
                    assemblyItems,
                    runtimeAssemblyItems,
                    std::nullopt,
                    std::nullopt,
                    std::nullopt,
                    std::vector<AnnotatedEventSignature>{}
                });
        }
        mappedContracts.insert(std::make_pair(sourceName, compiled));
    }

    return std::make_pair(mappedContracts, m_stack.errors());
}

std::vector<AnnotatedEventSignature> InternalCompiler::generateEventSignatures(
    std::string const& _contractName
) const
{
    std::vector<AnnotatedEventSignature> signatures;
    ContractDefinition const& contract = m_stack.contractDefinition(_contractName);

    for (EventDefinition const* event: contract.events() + contract.usedInterfaceEvents())
    {
        AnnotatedEventSignature eventInfo;
        auto eventFunctionType = event->functionType(true);

        eventInfo.signature = eventFunctionType->externalSignature();
        for (auto const& param: event->parameters())
            if (param->isIndexed())
                eventInfo.indexedTypes.emplace_back(param->type()->toString(true));
            else
                eventInfo.nonIndexedTypes.emplace_back(param->type()->toString(true));
        eventInfo.isAnonymous = event->isAnonymous();

        signatures.push_back(eventInfo);
    }

    return signatures;
}

std::string InternalCompiler::formatErrorInformation() const
{
    std::string errorInformation;
    for (auto const& error: m_stack.errors())
    {
        auto formatted = SourceReferenceFormatter::formatErrorInformation(
            *error.get(),
            m_stack,
            true,
            false
        );
        errorInformation.append(formatted);
    }

    return errorInformation;
}
