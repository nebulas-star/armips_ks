#include "Archs/ARM/Arm.h"

#include "Archs/ARM/ArmElfRelocator.h"
#include "Archs/ARM/ArmExpressionFunctions.h"
#include "Archs/ARM/ArmParser.h"
#include "Archs/ARM/CArmInstruction.h"
#include "Archs/ARM/CThumbInstruction.h"
#include "Archs/ARM/KeystoneArmInstruction.h"
#include "Core/Common.h"
#include "Core/Misc.h"

CArmArchitecture Arm;

CArmArchitecture::CArmArchitecture()
{
	clear();
}

CArmArchitecture::~CArmArchitecture()
{
	clear();
}

std::unique_ptr<CAssemblerCommand> CArmArchitecture::parseDirective(Parser& parser)
{
	ArmParser armParser;

	return armParser.parseDirective(parser);
}

std::unique_ptr<CAssemblerCommand> CArmArchitecture::parseOpcode(Parser& parser)
{
	ArmParser armParser;
	if (isArmV7A())
	{
#ifdef ARMIPS_HAS_KEYSTONE
		return armParser.parseKeystoneOpcode(parser, thumb, getEndianness());
#else
		Logger::printError(Logger::Error, "ARMv7-A support was not enabled at build time");
		return nullptr;
#endif
	}

	if (thumb)
		return armParser.parseThumbOpcode(parser);
	else
		return armParser.parseArmOpcode(parser);
}

void CArmArchitecture::registerExpressionFunctions(ExpressionFunctionHandler &handler)
{
	registerArmExpressionFunctions(handler);
}

void CArmArchitecture::clear()
{
	currentPoolContent.clear();
	thumb = false;
#ifdef ARMIPS_HAS_KEYSTONE
	resetKeystoneArmParserState();
#endif
}

void CArmArchitecture::Pass2()
{
	currentPoolContent.clear();
}

void CArmArchitecture::Revalidate()
{
	for (ArmPoolEntry& entry: currentPoolContent)
	{
		entry.command->applyFileInfo();
		Logger::queueError(Logger::Error, "Unable to find literal pool");
	}

	currentPoolContent.clear();
}

void CArmArchitecture::NextSection()
{

}

std::unique_ptr<IElfRelocator> CArmArchitecture::getElfRelocator()
{
	return std::make_unique<ArmElfRelocator>(version != AARCH_GBA);
}

void CArmArchitecture::addPoolValue(ArmOpcodeCommand* command, int32_t value)
{
	ArmPoolEntry entry;
	entry.command = command;
	entry.value = value;

	currentPoolContent.push_back(entry);
}
