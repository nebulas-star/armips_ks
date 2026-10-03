#include "Commands/CDirectiveMessage.h"

#include "Core/Common.h"
#include "Core/Misc.h"
#include "Core/SymbolData.h"

#include <utility>

CDirectiveMessage::CDirectiveMessage(Type type, Expression exp)
	: errorType(type), exp(exp)
{
}

bool CDirectiveMessage::Validate(const ValidateState &state)
{
	StringLiteral text;
	if (!exp.evaluateString(text,true))
	{
		Logger::queueError(Logger::Error, "Invalid expression");
		return false;
	}

	switch (errorType)
	{
	case Type::Warning:
		Logger::queueError(Logger::Warning,text.string());
		break;
	case Type::Error:
		Logger::queueError(Logger::Error,text.string());
		break;
	case Type::Notice:
		Logger::queueError(Logger::Notice,text.string());
		break;
	}
	return false;
}

CDirectiveInfo::CDirectiveInfo(std::vector<Expression> expressions)
	: expressions(std::move(expressions))
{
}

bool CDirectiveInfo::Validate(const ValidateState &state)
{
	message.clear();
	if (!g_infoLog.isEnabled())
		return false;

	for (Expression& expression: expressions)
	{
		StringLiteral text;
		if (!expression.evaluateString(text,true))
		{
			Logger::queueError(Logger::Error, "Invalid expression");
			continue;
		}

		message += text.string();
	}

	return false;
}

void CDirectiveInfo::Encode() const
{
	g_infoLog.writeLine(message);
}

void CDirectiveSym::writeSymData(SymbolData &symData) const
{
	symData.setEnabled(enabled);
}
