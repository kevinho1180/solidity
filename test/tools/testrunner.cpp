#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <test/EVMHost.h>

using namespace solidity;
using namespace test;
using namespace util;

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cerr << "Usage: " << argv[0] << " <evmone> <testtracefile>" << std::endl;
		return 1;
	}

	std::ifstream f(argv[2]);
	nlohmann::json testtrace = nlohmann::json::parse(f);

	evmc::VM& vm = EVMHost::getVM(argv[1]);

	for(auto&& [filename, testdata]: testtrace.items())
	{
		std::cout << filename << std::endl;
		std::unique_ptr<EVMHost> m_evmcHost;

		m_evmcHost = std::make_unique<EVMHost>(langutil::EVMVersion{}, vm);

		auto account = [](size_t i) {
			return h160(h256(u256{"0x1212121212121212121212121212120000000012"} + i * 0x1000), h160::AlignRight);
		};

		for (size_t i = 0; i < 10; i++)
			m_evmcHost->accounts[EVMHost::convertToEVMC(account(i))].balance =
				EVMHost::convertToEVMC(u256(1) << 100);

		bytes bytecode = util::fromHex(testdata["bytecode"]);
		util::h160 sender = account(0);
		util::h160 contractAddress;
		unsigned i = 0;
		for (auto& test: testdata["tests"])
		{
			m_evmcHost->newBlock();
			evmc_message message{};
			bytes input = util::fromHex(test["input"]["calldata"].get<std::string>());
			message.sender = EVMHost::convertToEVMC(sender);
			message.value = EVMHost::convertToEVMC(u256(test["input"]["value"].get<std::string>()));
			auto kind = test["kind"].get<std::string>();
			if (kind == "constructor")
			{
				input = bytecode + input;
				message.input_data = input.data();
				message.input_size = input.size();

				message.kind = EVMC_CREATE;
				message.recipient = {};
				message.code_address = {};
			}
			else if (kind == "call")
			{
				message.input_data = input.data();
				message.input_size = input.size();
				message.kind = EVMC_CALL;
				message.recipient = EVMHost::convertToEVMC(contractAddress);
				message.code_address = message.recipient;
			}
			else
			{
				std::cerr << "Unrecognized kind: " << kind << std::endl;
				return 1;
			}

			message.gas = 100000000;

			evmc::Result result = m_evmcHost->call(message);

			auto output = bytes(result.output_data, result.output_data + result.output_size);
			if (kind == "constructor")
			{
				contractAddress = EVMHost::convertFromEVMC(result.create_address);
			}

			bool status = (result.status_code == EVMC_SUCCESS);

			if (kind == "constructor")
			{
				if (!status)
				{
					std::cerr << "Creation failed." << std::endl;
					return 1;
				}
			}
			else
			{
				auto expectedStatus = test["output"]["status"].get<std::string>();
				if (expectedStatus == "failure")
				{
					if (status)
					{
						std::cerr << "Expected failure but got success" << std::endl;
						return 1;
					}
				}
				else if (expectedStatus == "success")
				{
					if (!status)
					{
						std::cerr << "Expected success but got failure" << std::endl;
						return 1;
					}
				}
				auto expectedOutput = test["output"]["returndata"].get<std::string>();
				if (output != util::fromHex(expectedOutput))
				{
					std::cerr << "Expected " << expectedOutput << " but got " << util::toHex(output) << std::endl;
					return 1;
				}
			}
			i++;
		}
		std::cout << "  => " << i << " tests passed." << std::endl;
	}
	return 0;
}