#include "PlaygroundEngine/Log.h"

#include <doctest/doctest.h>

#include <unistd.h>

import std;
import PlaygroundEngine.Files;
import PlaygroundEngine.Log;
import PlaygroundEngine.Paths;

namespace
{
	std::filesystem::path MakeTestDirectory(const std::string_view name)
	{
		// The process id keeps two users, or two CI jobs, off each other's directory in the shared temp.

		const std::filesystem::path directory =
			std::filesystem::temp_directory_path() / std::format("PlaygroundEngineLogTests-{}-{}", name, ::getpid());

		std::error_code errorCode;
		std::filesystem::remove_all(directory, errorCode);
		std::filesystem::create_directories(directory, errorCode);

		return directory;
	}

	void WriteEmptyFile(const std::filesystem::path& path)
	{
		REQUIRE(PgE::WriteBinaryFile(path, {}).has_value());
	}
}

// Characterizes the parser that turns source_location::function_name() into the qualified name shown in
// every log line. Inputs are literal GCC-16 signature strings (compiler-specific), so these also act as
// a tripwire for toolchain churn that would silently corrupt logged names.
TEST_CASE("ExtractQualifiedName reduces a signature to its qualified name")
{
	using PgE::detail::ExtractQualifiedName;

	// Return type stripped, parameter list dropped.
	CHECK(ExtractQualifiedName("void PgE::Foo()") == "PgE::Foo");
	CHECK(ExtractQualifiedName("int PgE::Bar::Baz()") == "PgE::Bar::Baz");

	// A constructor has no return type (no depth-0 space before the name).
	CHECK(ExtractQualifiedName("PgE::Bar::Bar()") == "PgE::Bar::Bar");

	// Template arguments must not be mistaken for the return-type space or params '('.
	CHECK(ExtractQualifiedName("void PgE::Container<int>::Add(int)") == "PgE::Container<int>::Add");
	CHECK(ExtractQualifiedName("std::vector<int> PgE::Foo::Get()") == "PgE::Foo::Get");

	// A leading '*'/'&' from a pointer/reference return type is trimmed off the name.
	CHECK(ExtractQualifiedName("int *PgE::Foo::Get()") == "PgE::Foo::Get");

	// GCC decorates module-local entities with "@Module.Name"; it is skipped.
	CHECK(ExtractQualifiedName("void PgE::Foo@PlaygroundEngine.Log::Bar()") == "PgE::Foo::Bar");
}

// Configure is the composition root's only knob on the logger, and the file sink is what makes a crashed
// run readable afterwards, so the check is that a line written through the macros actually lands on disk.
TEST_CASE("Configure installs a file sink that receives log output")
{
	const std::filesystem::path directory = MakeTestDirectory("sink");
	const std::filesystem::path logPath = directory / "PlaygroundEngine-test.log";

	PgE::Log::Configure(PgE::LogConfiguration{.FilePath = logPath});

	PGE_LOG(Warn, "file sink test {}", 42);
	PgE::Log::Flush();

	// Restored before any check, so a failure here cannot leave the rest of the suite logging into a
	// temporary directory that is about to be deleted.

	PgE::Log::Configure(PgE::LogConfiguration{});

	const std::expected<std::string, PgE::FileError> contents = PgE::ReadTextFile(logPath);
	REQUIRE(contents.has_value());

	CHECK(contents->contains("file sink test 42"));
	CHECK(contents->contains("warning"));

	// The file pattern carries the date the console pattern leaves out, in UTC, which is what makes it
	// agree with the UTC timestamp GenerateLogPath puts in the file's name. spdlog renders local time
	// unless told otherwise, so this fails if the formatter is ever built with the default.

	CHECK(contents->contains(std::format("{:%Y-%m-%d}", std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()))));
	CHECK(contents->contains("Z]"));

	// Configure replaces its own sink instead of stacking, so a line written after the restore above
	// must not reach the file that is no longer installed.

	PGE_LOG(Warn, "after restore");
	PgE::Log::Flush();

	CHECK(std::filesystem::file_size(logPath) == contents->size());

	std::error_code errorCode;
	std::filesystem::remove_all(directory, errorCode);
}

// Both of these are reachable only through the exported API, not through the engine's own call, which
// always hands over an absolute path into a directory it just created.
TEST_CASE("Configure survives the awkward paths its callers can hand it")
{
	const std::filesystem::path directory = MakeTestDirectory("configure");

	SUBCASE("a path with no directory component lands in the working directory")
	{
		const std::filesystem::path previousWorkingDirectory = std::filesystem::current_path();
		std::filesystem::current_path(directory);

		PgE::Log::Configure(PgE::LogConfiguration{.FilePath = "bare-name.log"});
		PgE::Log::Configure(PgE::LogConfiguration{});

		std::filesystem::current_path(previousWorkingDirectory);
		CHECK(std::filesystem::exists(directory / "bare-name.log"));
	}

	SUBCASE("a file that cannot be opened leaves the working one in place")
	{
		const std::filesystem::path logPath = directory / "PlaygroundEngine-kept.log";
		PgE::Log::Configure(PgE::LogConfiguration{.FilePath = logPath});

		// A directory cannot be opened as a log file, so this configure cannot install one.
		const std::filesystem::path unusablePath = directory / "unusable";
		std::filesystem::create_directories(unusablePath);

		PgE::Log::Configure(PgE::LogConfiguration{.FilePath = unusablePath});

		PGE_LOG(Warn, "still writing after a rejected reconfigure");
		PgE::Log::Flush();
		PgE::Log::Configure(PgE::LogConfiguration{});

		const std::expected<std::string, PgE::FileError> contents = PgE::ReadTextFile(logPath);
		REQUIRE(contents.has_value());
		CHECK(contents->contains("still writing after a rejected reconfigure"));
	}

	std::error_code errorCode;
	std::filesystem::remove_all(directory, errorCode);
}

// Resolving the default path and enforcing retention are the logger's own job: the composition root
// configures logging in one line and states no policy, so both have to hold with a default-constructed
// configuration.
TEST_CASE("Configure with no path logs to the executable's log directory")
{
	// Pins the layout the logger chose, which is the only place it is written down.
	const std::expected<std::filesystem::path, PgE::PathError> executableDirectory = PgE::GetExecutableDirectory();
	REQUIRE(executableDirectory.has_value());

	const std::filesystem::path logDirectory = *executableDirectory / "logs";

	PgE::Log::Configure(PgE::LogConfiguration{});
	PGE_LOG(Warn, "default path test");
	PgE::Log::Flush();

	std::vector<std::filesystem::path> logFiles;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(logDirectory))
	{
		if (entry.path().extension() == ".log")
		{
			logFiles.push_back(entry.path());
		}
	}
	REQUIRE_FALSE(logFiles.empty());

	std::ranges::sort(logFiles);

	const std::expected<std::string, PgE::FileError> contents = PgE::ReadTextFile(logFiles.back());
	REQUIRE(contents.has_value());

	CHECK(logFiles.back().filename().native().starts_with(PgE::LOG_FILE_PREFIX));

	// The path it resolved is the file's own first line, so the file names itself.
	CHECK(contents->contains(std::format("Logging to {}", logFiles.back().display_string())));
	CHECK(contents->contains("default path test"));
}

TEST_CASE("Configure drops the runs beyond MaximumFiles")
{
	const std::filesystem::path directory = MakeTestDirectory("retention");

	for (const std::string_view timestamp : {"20260101-000000-000", "20260102-000000-000", "20260103-000000-000", "20260104-000000-000"})
	{
		WriteEmptyFile(directory / std::format("{}-{}.log", PgE::LOG_FILE_PREFIX, timestamp));
	}

	// Sorts last, standing in for the file a real run would have just named.
	const std::filesystem::path logPath = directory / std::format("{}-20260105-000000-000.log", PgE::LOG_FILE_PREFIX);

	// None of these are this application's log files, so retention must not touch them.
	const std::filesystem::path siblingLog = directory / std::format("{}Editor-20260101-000000-000.log", PgE::LOG_FILE_PREFIX);
	const std::filesystem::path foreignLog = directory / "OtherApplication-20260101-000000-000.log";
	const std::filesystem::path foreignFile = directory / "notes.txt";
	const std::filesystem::path capture = directory / std::format("{}-20260101-000000-000.png", PgE::LOG_FILE_PREFIX);
	WriteEmptyFile(siblingLog);
	WriteEmptyFile(foreignLog);
	WriteEmptyFile(foreignFile);
	WriteEmptyFile(capture);

	PgE::Log::Configure(PgE::LogConfiguration{.FilePath = logPath, .MaximumFiles = 3});
	PgE::Log::Configure(PgE::LogConfiguration{});

	std::vector<std::filesystem::path> remainingLogs;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory))
	{
		if (entry.path().filename().native().starts_with(std::format("{}-", PgE::LOG_FILE_PREFIX)) && entry.path().extension() == ".log")
		{
			remainingLogs.push_back(entry.path().filename());
		}
	}
	std::ranges::sort(remainingLogs);

	// Three runs of history, this one included, and the surviving two are the newest of the old.
	CHECK(remainingLogs.size() == 3);
	CHECK(remainingLogs == std::vector<std::filesystem::path>{std::format("{}-20260103-000000-000.log", PgE::LOG_FILE_PREFIX),
															  std::format("{}-20260104-000000-000.log", PgE::LOG_FILE_PREFIX), logPath.filename()});

	CHECK(std::filesystem::exists(siblingLog));
	CHECK(std::filesystem::exists(foreignLog));
	CHECK(std::filesystem::exists(foreignFile));
	CHECK(std::filesystem::exists(capture));

	std::error_code errorCode;
	std::filesystem::remove_all(directory, errorCode);
}
