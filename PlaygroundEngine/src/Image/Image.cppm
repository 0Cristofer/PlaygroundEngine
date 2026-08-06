export module PlaygroundEngine.Image;

import std;

namespace PgE
{
	export enum class ImageError
	{
		EncodedDataTooLarge,
		DecodeFailed,
		DimensionsTooLarge,
		PixelCountMismatch,
		EncodeFailed,
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

	// Encoding only, symmetric with decoding: the PNG bytes come back to the caller, who writes them
	// through PlaygroundEngine.Files. Rows are assumed tightly packed at Width * BytesPerPixel, which
	// is what a buffer copied off the GPU with copyImageToBuffer already gives.

	export std::expected<std::vector<std::byte>, ImageError> EncodeImagePng(const Image& image);
}
