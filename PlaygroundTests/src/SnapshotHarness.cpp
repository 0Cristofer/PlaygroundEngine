module PlaygroundTests.SnapshotHarness;

import std;

namespace PgE::Snapshot
{
	namespace
	{
		std::filesystem::path SnapshotPath(const std::string_view name)
		{
			return std::filesystem::path(PGE_SNAPSHOT_DIR) / (std::string(name) + ".snap");
		}

		bool BlessRequested()
		{
			const char* value = std::getenv("PGE_BLESS");
			return value != nullptr && value[0] != '\0';
		}

		std::optional<std::string> ReadFile(const std::filesystem::path& path)
		{
			std::error_code error;
			const std::uintmax_t size = std::filesystem::file_size(path, error);
			if (error)
			{
				return std::nullopt;
			}

			std::FILE* file = std::fopen(path.string().c_str(), "rb");
			if (file == nullptr)
			{
				return std::nullopt;
			}

			std::string contents(static_cast<std::size_t>(size), '\0');
			const std::size_t read = std::fread(contents.data(), 1, contents.size(), file);
			std::fclose(file);

			contents.resize(read);
			return contents;
		}

		bool WriteFile(const std::filesystem::path& path, const std::string_view contents)
		{
			std::FILE* file = std::fopen(path.string().c_str(), "wb");
			if (file == nullptr)
			{
				return false;
			}

			std::fwrite(contents.data(), 1, contents.size(), file);
			std::fclose(file);
			return true;
		}

		// The first line that differs, as a one-based number plus both sides, so a failing snapshot points
		// straight at what moved instead of dumping the whole text.
		std::string FirstDifference(const std::string_view expected, const std::string_view actual)
		{
			const auto lines = [](std::string_view text) {
				std::vector<std::string_view> result;
				for (const auto line : std::views::split(text, '\n'))
				{
					result.emplace_back(line.begin(), line.end());
				}
				return result;
			};

			const std::vector<std::string_view> expectedLines = lines(expected);
			const std::vector<std::string_view> actualLines = lines(actual);
			const std::size_t count = std::max(expectedLines.size(), actualLines.size());

			for (std::size_t index = 0; index < count; ++index)
			{
				const std::string_view expectedLine = index < expectedLines.size() ? expectedLines[index] : "<missing>";
				const std::string_view actualLine = index < actualLines.size() ? actualLines[index] : "<missing>";

				if (expectedLine != actualLine)
				{
					return std::format("line {}:\n  expected: {}\n  actual:   {}", index + 1, expectedLine, actualLine);
				}
			}

			return "content differs only in trailing bytes";
		}
	}

	SnapshotResult CheckSnapshot(const std::string_view name, const std::string_view actual)
	{
		const std::filesystem::path path = SnapshotPath(name);

		if (BlessRequested())
		{
			std::error_code error;
			std::filesystem::create_directories(path.parent_path(), error);
			if (!WriteFile(path, actual))
			{
				return {.Matched = false, .Detail = std::format("could not write snapshot '{}'", name)};
			}

			return {.Matched = true, .Detail = std::format("blessed snapshot '{}'", name)};
		}

		const std::optional<std::string> expected = ReadFile(path);
		if (!expected)
		{
			return {.Matched = false, .Detail = std::format("no snapshot '{}'; run with PGE_BLESS=1 to create it", name)};
		}

		if (*expected == actual)
		{
			return {.Matched = true, .Detail = {}};
		}

		return {.Matched = false, .Detail = std::format("snapshot '{}' mismatch at {}", name, FirstDifference(*expected, actual))};
	}
}
