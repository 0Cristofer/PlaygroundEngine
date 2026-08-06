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

TEST_CASE("EncodeImagePng round-trips through DecodeImage")
{
	// PNG is lossless, so the decoded pixels have to match what was encoded byte for byte. A
	// non-uniform image with a varying alpha channel, so a dropped or reordered channel shows up
	// rather than being masked by symmetry.

	const PgE::Image original{.Pixels = {std::byte{10}, std::byte{20}, std::byte{30}, std::byte{255}, std::byte{40}, std::byte{50}, std::byte{60},
										 std::byte{128}, std::byte{70}, std::byte{80}, std::byte{90}, std::byte{64}, std::byte{100}, std::byte{110},
										 std::byte{120}, std::byte{255}},
							  .Width = 2,
							  .Height = 2};

	const std::expected<std::vector<std::byte>, PgE::ImageError> encoded = PgE::EncodeImagePng(original);
	REQUIRE(encoded.has_value());

	const std::expected<PgE::Image, PgE::ImageError> decoded = PgE::DecodeImage(encoded.value());
	REQUIRE(decoded.has_value());

	CHECK(decoded->Width == original.Width);
	CHECK(decoded->Height == original.Height);
	CHECK(decoded->Pixels == original.Pixels);
}

TEST_CASE("EncodeImagePng rejects a pixel buffer that does not match the dimensions")
{
	// The guard that matters for a GPU readback: a buffer sized for a stale extent would otherwise
	// be encoded as a skewed image rather than reported.

	const PgE::Image mismatched{.Pixels = std::vector<std::byte>(2 * 2 * PgE::Image::BytesPerPixel), .Width = 4, .Height = 4};

	const std::expected<std::vector<std::byte>, PgE::ImageError> result = PgE::EncodeImagePng(mismatched);

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error() == PgE::ImageError::PixelCountMismatch);
}

TEST_CASE("EncodeImagePng rejects dimensions that overflow the encoder's own arithmetic")
{
	// Neither axis alone overflows stb's int sizing; their product does. Rejected before the pixel
	// count is looked at, so the case costs no allocation.

	const std::expected<std::vector<std::byte>, PgE::ImageError> result =
		PgE::EncodeImagePng(PgE::Image{.Pixels = {}, .Width = 65536, .Height = 16384});

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error() == PgE::ImageError::DimensionsTooLarge);
}

TEST_CASE("EncodeImagePng rejects a zero-extent image")
{
	const std::expected<std::vector<std::byte>, PgE::ImageError> result = PgE::EncodeImagePng(PgE::Image{});

	REQUIRE_FALSE(result.has_value());
	CHECK(result.error() == PgE::ImageError::EncodeFailed);
}
