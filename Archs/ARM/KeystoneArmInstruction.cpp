#include "Archs/ARM/KeystoneArmInstruction.h"
#include "Archs/ARM/ArmParser.h"

#ifdef ARMIPS_HAS_KEYSTONE

#include "Core/Common.h"
#include "Core/FileManager.h"
#include "Core/Misc.h"
#include "Parser/Parser.h"

#include <algorithm>
#include <array>
#include <keystone/keystone.h>
#include <sstream>

namespace
{
	struct KeystoneEngines
	{
		std::array<ks_engine*, 4> handles{};
	};

	KeystoneEngines& engines()
	{
		// Keystone warns that closing one handle can invalidate process-wide
		// caches used by other handles. Keep these four handles for process life.
		static auto* value = new KeystoneEngines();
		return *value;
	}

	size_t engineIndex(bool thumb, Endianness endianness)
	{
		return (thumb ? 1u : 0u) | (endianness == Endianness::Big ? 2u : 0u);
	}

	ks_engine* getEngine(bool thumb, Endianness endianness)
	{
		ks_engine*& engine = engines().handles[engineIndex(thumb, endianness)];
		if (engine != nullptr)
			return engine;

		int mode = thumb ? KS_MODE_THUMB : KS_MODE_ARM;
		if (endianness == Endianness::Big)
			mode |= KS_MODE_BIG_ENDIAN;

		if (ks_open(KS_ARCH_ARM, mode, &engine) != KS_ERR_OK)
			engine = nullptr;
		return engine;
	}

	bool isItMnemonic(const std::string& mnemonic)
	{
		if (mnemonic.size() < 2 || mnemonic.size() > 5 || mnemonic[0] != 'i' || mnemonic[1] != 't')
			return false;

		return std::all_of(mnemonic.begin() + 2, mnemonic.end(), [](char value) {
			return value == 't' || value == 'e';
		});
	}

	std::string formatKeystoneInteger(int64_t value)
	{
		std::ostringstream stream;
		if (value < 0)
		{
			stream << "-0x" << std::hex << (uint64_t{0} - static_cast<uint64_t>(value));
		}
		else
		{
			stream << "0x" << std::hex << static_cast<uint64_t>(value);
		}
		return stream.str();
	}

	struct ParsedStatement
	{
		std::string text;
		std::string mnemonic;
		bool hasLiteralPoolSyntax = false;
		bool hasUnsupportedLabel = false;
	};

	ParsedStatement readStatement(Parser& parser)
	{
		ParsedStatement result;
		bool first = true;

		while (!parser.atEnd() && parser.peekToken().type != TokenType::Separator)
		{
			const Token token = parser.nextToken();
			if (!result.text.empty())
				result.text += ' ';

			if (token.type == TokenType::Identifier)
			{
				const std::string& identifier = token.identifierValue().string();
				result.text += identifier;
				if (first)
					result.mnemonic = identifier;
				if (!identifier.empty() && identifier[0] == '@')
					result.hasUnsupportedLabel = true;
			}
			else if (token.type == TokenType::Integer)
			{
				// Configuring Keystone's symbol resolver changes its lexer to
				// radix 16. Preserve armips' already parsed numeric value by
				// passing an explicitly based literal instead of the source text.
				result.text += formatKeystoneInteger(token.intValue());
			}
			else
			{
				result.text += token.getOriginalText();
			}

			if (token.type == TokenType::Assign)
				result.hasLiteralPoolSyntax = true;
			first = false;
		}

		if (!parser.atEnd())
			parser.eatToken();
		return result;
	}

	std::shared_ptr<KeystoneItBlock> activeItBlock;
}

thread_local CKeystoneArmInstruction* CKeystoneArmInstruction::activeCommand = nullptr;

CKeystoneArmInstruction::CKeystoneArmInstruction(std::string source, bool thumb,
	Endianness endianness, std::shared_ptr<KeystoneItBlock> itBlock, size_t itIndex)
	: source(std::move(source)), thumb(thumb), endianness(endianness),
	  itBlock(std::move(itBlock)), itIndex(itIndex)
{
}

bool CKeystoneArmInstruction::resolveSymbol(const char* symbol, uint64_t* value)
{
	return activeCommand != nullptr && activeCommand->resolveSymbolForCommand(symbol, value);
}

bool CKeystoneArmInstruction::resolveSymbolForCommand(const char* symbol, uint64_t* value)
{
	Identifier name(symbol);
	if (name.size() == 0 || name.startsWith('@'))
		return false;

	std::shared_ptr<Label> label = Global.symbolTable.getLabel(name, FileNum, getSection());
	if (label == nullptr)
		return false;

	if (!label->isDefined())
	{
		if (std::find(undefinedSymbols.begin(), undefinedSymbols.end(), symbol) == undefinedSymbols.end())
			undefinedSymbols.emplace_back(symbol);

		// A nearby provisional value lets branch encodings establish their size
		// on the first pass. A later pass replaces it with the defined address.
		*value = static_cast<uint64_t>(address);
	}
	else
	{
		*value = static_cast<uint64_t>(label->getValue());
	}
	return true;
}

std::string CKeystoneArmInstruction::assemblySource() const
{
	if (itBlock == nullptr)
		return source;

	std::ostringstream stream;
	for (size_t index = 0; index <= itIndex && index < itBlock->statements.size(); ++index)
	{
		if (index != 0)
			stream << '\n';
		stream << itBlock->statements[index];
	}
	return stream.str();
}

bool CKeystoneArmInstruction::Validate(const ValidateState& state)
{
	address = g_fileManager->getVirtualAddress();
	undefinedSymbols.clear();
	if ((thumb && (address & 1) != 0) || (!thumb && (address & 3) != 0))
		Logger::queueError(Logger::Warning, thumb ? "Opcode not halfword aligned" : "Opcode not word aligned");

	const size_t oldSize = encoding.size();
	encoding.clear();

	if (itBlock != nullptr && itIndex == 0)
	{
		itBlock->startAddress = address;
		itBlock->encodedPrefixSize = 0;
		itBlock->prefixValid = true;
		if (itBlock->statements.size() != itBlock->expectedStatementCount)
		{
			Logger::queueError(Logger::Error, "Incomplete Thumb IT block");
			itBlock->prefixValid = false;
		}
	}

	if (itBlock != nullptr && !itBlock->prefixValid)
		return oldSize != 0;

	ks_engine* engine = getEngine(thumb, endianness);
	if (engine == nullptr)
	{
		Logger::queueError(Logger::Error, "Unable to initialize the Keystone ARM engine");
		return oldSize != 0;
	}

	if (ks_option(engine, KS_OPT_SYM_RESOLVER, reinterpret_cast<size_t>(&CKeystoneArmInstruction::resolveSymbol)) != KS_ERR_OK)
	{
		Logger::queueError(Logger::Error, "Unable to configure the Keystone symbol resolver");
		return oldSize != 0;
	}

	uint64_t assemblyAddress = static_cast<uint64_t>(address);
	if (itBlock != nullptr)
	{
		assemblyAddress = static_cast<uint64_t>(itBlock->startAddress);
		const int64_t expectedAddress = itBlock->startAddress + static_cast<int64_t>(itBlock->encodedPrefixSize);
		if (address != expectedAddress)
		{
			Logger::queueError(Logger::Error, "Thumb IT block instructions must be consecutive");
			itBlock->prefixValid = false;
			return oldSize != 0;
		}
	}

	unsigned char* assembled = nullptr;
	size_t assembledSize = 0;
	size_t statementCount = 0;
	const std::string input = assemblySource();
	activeCommand = this;
	const int result = ks_asm(engine, input.c_str(), assemblyAddress,
		&assembled, &assembledSize, &statementCount);

	// In unified Thumb syntax, Keystone preserves CPSR for an unsuffixed MOV
	// immediate and consequently selects MOV.W. armips traditionally selects
	// the compact Thumb encoding when one exists. Assemble MOVS as a candidate
	// and use it only when it is a single 16-bit instruction; an explicit .w
	// and forms which require Thumb-2 remain unchanged.
	if (result == 0 && thumb && itBlock == nullptr && assembledSize > 2 &&
		source.compare(0, 4, "mov ") == 0)
	{
		const std::string narrowInput = "movs" + input.substr(3);
		unsigned char* narrowAssembled = nullptr;
		size_t narrowSize = 0;
		size_t narrowStatementCount = 0;
		const int narrowResult = ks_asm(engine, narrowInput.c_str(), assemblyAddress,
			&narrowAssembled, &narrowSize, &narrowStatementCount);
		if (narrowResult == 0 && narrowSize == 2)
		{
			ks_free(assembled);
			assembled = narrowAssembled;
			assembledSize = narrowSize;
			statementCount = narrowStatementCount;
			narrowAssembled = nullptr;
		}
		if (narrowAssembled != nullptr)
			ks_free(narrowAssembled);
	}
	activeCommand = nullptr;

	if (result != 0)
	{
		const ks_err error = ks_errno(engine);
		Logger::queueError(Logger::Error, "Keystone ARM assembly failed for \"%s\": %s",
			source, ks_strerror(error));
		if (itBlock != nullptr)
			itBlock->prefixValid = false;
		if (assembled != nullptr)
			ks_free(assembled);
		return oldSize != 0;
	}

	const size_t prefixSize = itBlock == nullptr ? 0 : itBlock->encodedPrefixSize;
	if (assembledSize < prefixSize)
	{
		Logger::queueError(Logger::Error, "Keystone returned an invalid Thumb IT block encoding");
		if (itBlock != nullptr)
			itBlock->prefixValid = false;
	}
	else
	{
		encoding.assign(assembled + prefixSize, assembled + assembledSize);
		if (itBlock != nullptr)
			itBlock->encodedPrefixSize = assembledSize;
	}
	ks_free(assembled);

	if (encoding.empty())
	{
		Logger::queueError(Logger::Error,
			"Keystone produced no code for \"%s\"; Thumb conditional instructions require an IT block",
			source);
	}

	for (const std::string& symbol : undefinedSymbols)
		Logger::queueError(Logger::Error, "Undefined label \"%s\"", symbol);

	g_fileManager->advanceMemory(encoding.size());
	return oldSize != encoding.size();
}

void CKeystoneArmInstruction::Encode() const
{
	if (!encoding.empty())
		g_fileManager->write(const_cast<unsigned char*>(encoding.data()), encoding.size());
}

void CKeystoneArmInstruction::writeTempData(TempData& tempData) const
{
	tempData.writeLine(address, "   " + source);
}

std::unique_ptr<CAssemblerCommand> parseKeystoneArmOpcode(
	Parser& parser, bool thumb, Endianness endianness)
{
	if (parser.peekToken().type != TokenType::Identifier)
		return nullptr;

	const Token firstToken = parser.peekToken();
	ParsedStatement statement = readStatement(parser);
	if (statement.hasUnsupportedLabel)
	{
		parser.printError(firstToken, "@StaticLabel and @@LocalLabel are not supported by the ARMv7-A backend");
		return std::make_unique<InvalidCommand>();
	}
	if (statement.hasLiteralPoolSyntax)
	{
		parser.printError(firstToken, "Literal-pool syntax (ldr ..., =value) is not supported by the ARMv7-A backend");
		return std::make_unique<InvalidCommand>();
	}
	if (parser.isInitializingMacro())
		return std::make_unique<DummyCommand>();

	if (activeItBlock != nullptr)
	{
		if (!thumb)
		{
			parser.printError(firstToken, "Thumb IT block cannot continue in ARM mode");
			activeItBlock.reset();
			return std::make_unique<InvalidCommand>();
		}
		if (isItMnemonic(statement.mnemonic))
		{
			parser.printError(firstToken, "Nested Thumb IT blocks are not supported");
			activeItBlock.reset();
			return std::make_unique<InvalidCommand>();
		}

		activeItBlock->statements.push_back(statement.text);
		const size_t index = activeItBlock->statements.size() - 1;
		auto command = std::make_unique<CKeystoneArmInstruction>(
			statement.text, thumb, endianness, activeItBlock, index);
		if (activeItBlock->statements.size() == activeItBlock->expectedStatementCount)
			activeItBlock.reset();
		return command;
	}

	if (thumb && isItMnemonic(statement.mnemonic))
	{
		activeItBlock = std::make_shared<KeystoneItBlock>();
		activeItBlock->expectedStatementCount = statement.mnemonic.size();
		activeItBlock->statements.push_back(statement.text);
		return std::make_unique<CKeystoneArmInstruction>(
			statement.text, thumb, endianness, activeItBlock, 0);
	}

	return std::make_unique<CKeystoneArmInstruction>(statement.text, thumb, endianness);
}

void resetKeystoneArmParserState()
{
	activeItBlock.reset();
}

std::unique_ptr<CAssemblerCommand> ArmParser::parseKeystoneOpcode(
	Parser& parser, bool thumb, Endianness endianness)
{
	return parseKeystoneArmOpcode(parser, thumb, endianness);
}

#endif
