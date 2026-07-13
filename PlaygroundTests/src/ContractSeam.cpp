import std;
import PlaygroundTests.ContractSeam;

// The single, program-wide replaceable contract-violation handler for the test executable. It lives in
// a non-module TU so it keeps the global external linkage the contract runtime resolves against, and
// throwing surfaces a violation as a catchable exception before the guarded body runs.
void handle_contract_violation(const std::contracts::contract_violation& info)
{
	throw PgETest::ContractViolationError(info.comment());
}
