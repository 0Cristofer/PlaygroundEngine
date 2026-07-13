export module PlaygroundTests.ContractSeam;

import std;

// The test build's contract-violation seam. An enforced violation normally terminates and cannot be
// caught by doctest, so the test build's handler (ContractSeam.cpp) throws this instead. Tests assert
// rejection with CHECK_THROWS_AS. See docs/CorrectnessAndStandards.md Section 4.
namespace PgETest
{
	export struct ContractViolationError : std::runtime_error
	{
		explicit ContractViolationError(const std::string& predicate) : std::runtime_error(predicate)
		{}
	};
}
