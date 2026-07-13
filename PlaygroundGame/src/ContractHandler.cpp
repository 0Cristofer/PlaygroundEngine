import std;
import PlaygroundEngine.Diagnostics;

// The game's runtime contract-violation handler. Defined in a non-module translation unit so it keeps
// the global external linkage the contract runtime resolves against. It logs through the engine, then
// returns; under the enforce semantic the runtime terminates without unwinding.
void handle_contract_violation(const std::contracts::contract_violation& violation)
{
	PgE::LogContractViolation(violation);
}
