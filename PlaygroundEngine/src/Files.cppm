export module PlaygroundEngine.Files;

import std;

namespace PgE
{
	export enum class FileError
	{
		UnableToOpen,
		UnableToDetermineSize,
		UnableToRead,
		UnableToWrite,
	};

	// Whole-file reads for content small enough to sit in memory at once, which covers compiled
	// shaders and .obj meshes. Both open in binary mode, so a text read returns the bytes on disk
	// with no line-ending translation.

	export std::expected<std::vector<std::byte>, FileError> ReadBinaryFile(const std::filesystem::path& path);
	export std::expected<std::string, FileError> ReadTextFile(const std::filesystem::path& path);

	// Whole-file write, truncating an existing file. Parent directories are expected to exist; a
	// missing one surfaces as UnableToOpen rather than being created, so a mistyped path fails
	// instead of littering the filesystem.

	export std::expected<void, FileError> WriteBinaryFile(const std::filesystem::path& path, std::span<const std::byte> bytes);
}
