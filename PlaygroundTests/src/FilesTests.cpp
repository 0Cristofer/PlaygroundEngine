#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Files;

namespace
{
	// Each case gets its own path under the system temp directory, so cases cannot read back a file
	// a neighbour left behind.

	std::filesystem::path MakeTemporaryPath(const std::string_view caseName)
	{
		return std::filesystem::temp_directory_path() / std::format("PlaygroundTests-{}.bin", caseName);
	}

	struct TemporaryFile
	{
		explicit TemporaryFile(const std::string_view caseName) : Path(MakeTemporaryPath(caseName))
		{
			std::error_code errorCode;
			std::filesystem::remove(Path, errorCode);
		}

		// The error_code overload deliberately: a failing REQUIRE throws to abort the case, and a
		// throwing remove during that unwinding would escape this noexcept destructor and turn an
		// assertion failure into a terminated test binary with no report.

		~TemporaryFile()
		{
			std::error_code errorCode;
			std::filesystem::remove(Path, errorCode);
		}

		TemporaryFile(const TemporaryFile&) = delete;
		TemporaryFile& operator=(const TemporaryFile&) = delete;

		std::filesystem::path Path;
	};
}

TEST_CASE("WriteBinaryFile round-trips through ReadBinaryFile")
{
	const TemporaryFile file("round-trip");

	// Includes a zero byte and a byte above 0x7F, which is what a text-mode write or a signed-char
	// conversion would truncate or reinterpret.

	const std::vector contents = {std::byte{0x00}, std::byte{0x01}, std::byte{0x7F}, std::byte{0x80}, std::byte{0xFF}};

	const std::expected<void, PgE::FileError> writeResult = PgE::WriteBinaryFile(file.Path, contents);
	REQUIRE(writeResult.has_value());

	const std::expected<std::vector<std::byte>, PgE::FileError> readResult = PgE::ReadBinaryFile(file.Path);
	REQUIRE(readResult.has_value());
	CHECK(readResult.value() == contents);
}

TEST_CASE("WriteBinaryFile truncates an existing file")
{
	const TemporaryFile file("truncate");

	const std::vector longContents = {std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
	REQUIRE(PgE::WriteBinaryFile(file.Path, longContents).has_value());

	const std::vector shortContents = {std::byte{9}};
	REQUIRE(PgE::WriteBinaryFile(file.Path, shortContents).has_value());

	// The tail of the first write must be gone, not left behind past the second one.

	const std::expected<std::vector<std::byte>, PgE::FileError> readResult = PgE::ReadBinaryFile(file.Path);
	REQUIRE(readResult.has_value());
	CHECK(readResult.value() == shortContents);
}

TEST_CASE("WriteBinaryFile writes an empty file for an empty span")
{
	const TemporaryFile file("empty");

	const std::expected<void, PgE::FileError> writeResult = PgE::WriteBinaryFile(file.Path, {});
	REQUIRE(writeResult.has_value());

	REQUIRE(std::filesystem::exists(file.Path));
	CHECK(std::filesystem::file_size(file.Path) == 0);
}

TEST_CASE("WriteBinaryFile reports a failure when the directory does not exist")
{
	const std::filesystem::path path = std::filesystem::temp_directory_path() / "PlaygroundTests-absent-directory" / "file.bin";

	const std::expected<void, PgE::FileError> result = PgE::WriteBinaryFile(path, {});

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error() == PgE::FileError::UnableToOpen);
}
