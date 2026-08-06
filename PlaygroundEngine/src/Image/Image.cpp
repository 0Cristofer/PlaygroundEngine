module;

#include <stb_image.h>
#include <stb_image_write.h>

module PlaygroundEngine.Image;

import std;

namespace PgE
{
	namespace
	{
		struct StbPixelsDeleter
		{
			void operator()(stbi_uc* pixels) const
			{
				stbi_image_free(pixels);
			}
		};

		using StbPixels = std::unique_ptr<stbi_uc, StbPixelsDeleter>;

		// ReSharper disable once CppParameterMayBeConstPtrOrRef
		void AppendEncodedChunk(void* const context, void* const data, const int size)
		{
			auto* const encodedBytes = static_cast<std::vector<std::byte>*>(context);
			const auto* const chunk = static_cast<const std::byte*>(data);

			encodedBytes->insert(encodedBytes->end(), chunk, chunk + size);
		}
	}

	std::expected<Image, ImageError> DecodeImage(const std::span<const std::byte> encodedBytes)
	{
		// stb takes the encoded length as int, so a larger span would silently decode a truncated
		// prefix rather than fail.

		if (encodedBytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
		{
			return std::unexpected(ImageError::EncodedDataTooLarge);
		}

		int width = 0;
		int height = 0;
		int channelsInFile = 0;

		const StbPixels pixels(stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(encodedBytes.data()), static_cast<int>(encodedBytes.size()),
													 &width, &height, &channelsInFile, STBI_rgb_alpha));
		if (!pixels)
		{
			return std::unexpected(ImageError::DecodeFailed);
		}

		// channelsInFile reports what the file held, not what was produced; STBI_rgb_alpha means the
		// decoded buffer is always four channels wide.

		const auto* const decodedBytes = reinterpret_cast<const std::byte*>(pixels.get());
		const std::size_t decodedSize = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * Image::BytesPerPixel;

		return Image{.Pixels = std::vector(decodedBytes, decodedBytes + decodedSize),
					 .Width = static_cast<std::uint32_t>(width),
					 .Height = static_cast<std::uint32_t>(height)};
	}

	std::expected<std::vector<std::byte>, ImageError> EncodeImagePng(const Image& image)
	{
		// stb sizes its filter buffer with the int expression (width * channels + 1) * height, so the
		// whole product is what has to be bounded: a per-axis bound covers only the stride and still
		// admits a pair of dimensions that wraps once multiplied.

		const std::size_t filterBufferSize =
			(static_cast<std::size_t>(image.Width) * Image::BytesPerPixel + 1) * static_cast<std::size_t>(image.Height);

		if (filterBufferSize > static_cast<std::size_t>(std::numeric_limits<int>::max()))
		{
			return std::unexpected(ImageError::DimensionsTooLarge);
		}

		if (const std::size_t expectedSize = static_cast<std::size_t>(image.Width) * static_cast<std::size_t>(image.Height) * Image::BytesPerPixel;
			image.Pixels.size() != expectedSize)
		{
			return std::unexpected(ImageError::PixelCountMismatch);
		}

		// A zero-extent image satisfies the size check above with an empty buffer, so it has to be
		// rejected on its own: PNG has no representation for one.

		if (image.Width == 0 || image.Height == 0)
		{
			return std::unexpected(ImageError::EncodeFailed);
		}

		std::vector<std::byte> encodedBytes;

		const int writeResult =
			stbi_write_png_to_func(AppendEncodedChunk, &encodedBytes, static_cast<int>(image.Width), static_cast<int>(image.Height),
								   Image::BytesPerPixel, image.Pixels.data(), static_cast<int>(image.Width * Image::BytesPerPixel));
		if (writeResult == 0)
		{
			return std::unexpected(ImageError::EncodeFailed);
		}

		return encodedBytes;
	}
}
