module;

#include "PlaygroundEngine/Log.h"

module PlaygroundEngine.Diagnostics;

import std;
import PlaygroundEngine.Log;

namespace PgE
{
	std::string_view ContractKindText(const std::contracts::assertion_kind kind)
	{
		switch (kind)
		{
		case std::contracts::assertion_kind::pre:
			return "pre";
		case std::contracts::assertion_kind::post:
			return "post";
		case std::contracts::assertion_kind::assert:
			return "assert";
		default:
			return "contract";
		}
	}

	void LogContractViolation(const std::contracts::contract_violation& violation)
	{
		const std::string message =
			std::format("contract violation [{}]: {}", ContractKindText(violation.kind()), violation.comment());
		Log::Print(LogLevel::Fatal, violation.location(), message);
	}
}
