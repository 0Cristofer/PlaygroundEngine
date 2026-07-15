// ReSharper disable CppInconsistentNaming
import PlaygroundEngine.Diagnostics;
import std;

// The engine's default contract-violation handler, in its own TU so the archive member carries only this
// definition: an executable that defines its own handler keeps it (this member is then never pulled), and
// one that does not gets this. Non-module TU for the global linkage the contract runtime resolves against.
void handle_contract_violation(const std::contracts::contract_violation& violation)
{
	PgE::LogContractViolation(violation);
}
