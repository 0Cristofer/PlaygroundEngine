export module PlaygroundEngine.Diagnostics;

import std;

namespace PgE
{
	// Stable human-readable name for a contract-violation kind, shared by the handler and any
	// diagnostic that reports a violation.
	export std::string_view ContractKindText(std::contracts::assertion_kind kind);

	// Logs a C++26 contract violation through the engine logger at the violation's own source location.
	// Each executable's replaceable handler calls this; see docs/CorrectnessAndStandards.md Section 4.
	export void LogContractViolation(const std::contracts::contract_violation& violation);
}
