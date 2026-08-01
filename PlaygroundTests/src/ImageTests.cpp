#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Image;

namespace
{
	// A 2x2 truecolour PNG with no alpha channel: red, green on the first row, blue, white on the
	// second. Embedded rather than read from disk so the test exercises decoding alone, and
	// three-channel so it also pins the expansion to RGBA.

	constexpr std::array TwoByTwoPng = {
		std::byte{0x89}, std::byte{0x50}, std::byte{0x4E}, std::byte{0x47}, std::byte{0x0D}, std::byte{0x0A}, std::byte{0x1A}, std::byte{0x0A},
		std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x0D}, std::byte{0x49}, std::byte{0x48}, std::byte{0x44}, std::byte{0x52},
		std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x02},
		std::byte{0x08}, std::byte{0x02}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0xFD}, std::byte{0xD4}, std::byte{0x9A},
		std::byte{0x73}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x12}, std::byte{0x49}, std::byte{0x44}, std::byte{0x41},
		std::byte{0x54}, std::byte{0x78}, std::byte{0xDA}, std::byte{0x63}, std::byte{0xF8}, std::byte{0xCF}, std::byte{0xC0}, std::byte{0xC0},
		std::byte{0x00}, std::byte{0xC2}, std::byte{0x0C}, std::byte{0xFF}, std::byte{0x81}, std::byte{0x00}, std::byte{0x00}, std::byte{0x1F},
		std::byte{0xEE}, std::byte{0x05}, std::byte{0xFB}, std::byte{0xF1}, std::byte{0xAB}, std::byte{0xBA}, std::byte{0x77}, std::byte{0x00},
		std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x49}, std::byte{0x45}, std::byte{0x4E}, std::byte{0x44}, std::byte{0xAE},
		std::byte{0x42}, std::byte{0x60}, std::byte{0x82}};

	std::vector<int> ToChannelValues(const std::span<const std::byte> pixels)
	{
		std::vector<int> values;
		values.reserve(pixels.size());

		for (const std::byte pixel : pixels)
		{
			values.push_back(static_cast<int>(pixel));
		}

		return values;
	}
}

TEST_CASE("DecodeImage expands a three-channel PNG to RGBA")
{
	const std::expected<PgE::Image, PgE::ImageError> result = PgE::DecodeImage(TwoByTwoPng);

	REQUIRE(result.has_value());
	CHECK(result->Width == 2);
	CHECK(result->Height == 2);

	// Four channels per pixel regardless of what the file held, with the missing alpha filled opaque.

	REQUIRE(result->Pixels.size() == 2 * 2 * PgE::Image::BytesPerPixel);

	const std::vector expected = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
	CHECK(ToChannelValues(result->Pixels) == expected);
}

TEST_CASE("DecodeImage reports a failure for bytes that are not an image")
{
	constexpr std::array notAnImage = {std::byte{'n'}, std::byte{'o'}, std::byte{'p'}, std::byte{'e'}};

	const std::expected<PgE::Image, PgE::ImageError> result = PgE::DecodeImage(notAnImage);

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error() == PgE::ImageError::DecodeFailed);
}

TEST_CASE("DecodeImage reports a failure for an empty span")
{
	const std::expected<PgE::Image, PgE::ImageError> result = PgE::DecodeImage({});

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error() == PgE::ImageError::DecodeFailed);
}
