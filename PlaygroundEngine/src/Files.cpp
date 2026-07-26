module PlaygroundEngine.Files;

import std;

namespace PgE
{
	namespace
	{
		// ifstream despite the deny-list on iostreams: that entry targets formatted output, whose
		// costs (locales, ios_base::Init) a bulk read never pays, and the stream reports failure
		// through its state rather than by throwing, so -fno-exceptions is unaffected.

		template <typename Contents>
		std::expected<Contents, FileError> ReadWholeFile(const std::filesystem::path& path)
		{
			// Binary mode for both, so a text read yields the bytes on disk with no line-ending
			// translation. ate opens positioned at the end, making tellg the size.

			std::ifstream stream(path, std::ios::binary | std::ios::ate);
			if (!stream)
			{
				return std::unexpected(FileError::UnableToOpen);
			}

			const std::streamoff signedSize = stream.tellg();
			if (signedSize < 0)
			{
				return std::unexpected(FileError::UnableToDetermineSize);
			}

			stream.seekg(0);

			const auto size = static_cast<std::size_t>(signedSize);
			Contents contents(size, typename Contents::value_type{});

			if (size > 0 && !stream.read(reinterpret_cast<char*>(contents.data()), signedSize))
			{
				return std::unexpected(FileError::UnableToRead);
			}

			return contents;
		}
	}

	std::expected<std::vector<std::byte>, FileError> ReadBinaryFile(const std::filesystem::path& path)
	{
		return ReadWholeFile<std::vector<std::byte>>(path);
	}

	std::expected<std::string, FileError> ReadTextFile(const std::filesystem::path& path)
	{
		return ReadWholeFile<std::string>(path);
	}
}
