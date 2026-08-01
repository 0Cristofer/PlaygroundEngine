export module PlaygroundEngine.Image;

import std;

namespace PgE
{
	export enum class ImageError
	{
		EncodedDataTooLarge,
		DecodeFailed,
	};

	export struct Image
	{
		static constexpr std::uint32_t BytesPerPixel = 4;

		std::vector<std::byte> Pixels;
		std::uint32_t Width = 0;
		std::uint32_t Height = 0;
	};

	// Decoding only, with no file access of its own: a caller reads the bytes through
	// PlaygroundEngine.Files, which keeps FileError and ImageError distinct and leaves bytes from an
	// archive or the network needing no separate entry point.

	export std::expected<Image, ImageError> DecodeImage(std::span<const std::byte> encodedBytes);
}
