#pragma once

#include "Archs/ARM/Arm.h"

#include <memory>
#include <string>
#include <vector>

struct KeystoneItBlock
{
	size_t expectedStatementCount = 0;
	std::vector<std::string> statements;
	int64_t startAddress = 0;
	size_t encodedPrefixSize = 0;
	bool prefixValid = false;
};

class CKeystoneArmInstruction: public ArmOpcodeCommand
{
public:
	CKeystoneArmInstruction(std::string source, bool thumb, Endianness endianness,
		std::shared_ptr<KeystoneItBlock> itBlock = nullptr, size_t itIndex = 0);

	bool Validate(const ValidateState& state) override;
	void Encode() const override;
	void writeTempData(TempData& tempData) const override;
	void setPoolAddress(int64_t address) override { }

private:
	static bool resolveSymbol(const char* symbol, uint64_t* value);
	bool resolveSymbolForCommand(const char* symbol, uint64_t* value);
	std::string assemblySource() const;

	std::string source;
	bool thumb;
	Endianness endianness;
	std::shared_ptr<KeystoneItBlock> itBlock;
	size_t itIndex;
	std::vector<unsigned char> encoding;
	int64_t address = 0;
	std::vector<std::string> undefinedSymbols;

	static thread_local CKeystoneArmInstruction* activeCommand;
};

std::unique_ptr<CAssemblerCommand> parseKeystoneArmOpcode(
	Parser& parser, bool thumb, Endianness endianness);
void resetKeystoneArmParserState();
