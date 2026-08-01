module;

#include <stb_image.h>

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
}
