export module PlaygroundEngine.Files;

import std;

namespace PgE
{
	export enum class FileError
	{
		UnableToOpen,
		UnableToDetermineSize,
		UnableToRead,
	};

	// Whole-file reads for content small enough to sit in memory at once, which covers compiled
	// shaders and .obj meshes. Both open in binary mode, so a text read returns the bytes on disk
	// with no line-ending translation.

	export std::expected<std::vector<std::byte>, FileError> ReadBinaryFile(const std::filesystem::path& path);
	export std::expected<std::string, FileError> ReadTextFile(const std::filesystem::path& path);
}
