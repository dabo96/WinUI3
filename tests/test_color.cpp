#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Math/Color.h"

using namespace FluentUI;
using Catch::Matchers::WithinAbs;

TEST_CASE("Color default constructor", "[color]") {
    Color c;
    REQUIRE(c.r == 0.0f);
    REQUIRE(c.g == 0.0f);
    REQUIRE(c.b == 0.0f);
    REQUIRE(c.a == 1.0f);
}

TEST_CASE("Color parameterized constructor", "[color]") {
    Color c(0.1f, 0.2f, 0.3f, 0.4f);
    REQUIRE(c.r == 0.1f);
    REQUIRE(c.a == 0.4f);
}

TEST_CASE("Color arithmetic", "[color]") {
    Color a(0.2f, 0.3f, 0.4f, 1.0f);
    Color b(0.1f, 0.1f, 0.1f, 0.0f);

    Color sum = a + b;
    REQUIRE_THAT(sum.r, WithinAbs(0.3f, 1e-5));

    Color diff = a - b;
    REQUIRE_THAT(diff.r, WithinAbs(0.1f, 1e-5));

    Color scaled = a * 2.0f;
    REQUIRE_THAT(scaled.r, WithinAbs(0.4f, 1e-5));
}

TEST_CASE("Color ToUint32 / FromUint32 roundtrip", "[color]") {
    Color orig(1.0f, 0.0f, 0.5f, 1.0f);
    uint32_t packed = orig.ToUint32();
    Color unpacked = Color::FromUint32(packed);

    REQUIRE_THAT(unpacked.r, WithinAbs(1.0f, 1.0f / 255.0f));
    REQUIRE_THAT(unpacked.g, WithinAbs(0.0f, 1.0f / 255.0f));
    REQUIRE_THAT(unpacked.b, WithinAbs(0.5f, 1.0f / 255.0f));
    REQUIRE_THAT(unpacked.a, WithinAbs(1.0f, 1.0f / 255.0f));
}

TEST_CASE("Color ToUint32 specific values", "[color]") {
    Color white(1.0f, 1.0f, 1.0f, 1.0f);
    REQUIRE(white.ToUint32() == 0xFFFFFFFF);

    Color black(0.0f, 0.0f, 0.0f, 1.0f);
    REQUIRE(black.ToUint32() == 0xFF000000);

    Color transparent(0.0f, 0.0f, 0.0f, 0.0f);
    REQUIRE(transparent.ToUint32() == 0x00000000);
}

TEST_CASE("Color Luminance", "[color]") {
    Color white(1.0f, 1.0f, 1.0f, 1.0f);
    REQUIRE_THAT(white.Luminance(), WithinAbs(1.0f, 1e-4));

    Color black(0.0f, 0.0f, 0.0f, 1.0f);
    REQUIRE_THAT(black.Luminance(), WithinAbs(0.0f, 1e-4));

    Color red(1.0f, 0.0f, 0.0f, 1.0f);
    REQUIRE_THAT(red.Luminance(), WithinAbs(0.2126f, 1e-4));
}

TEST_CASE("Color Lerp", "[color]") {
    Color a(0.0f, 0.0f, 0.0f, 0.0f);
    Color b(1.0f, 1.0f, 1.0f, 1.0f);

    Color mid = Color::Lerp(a, b, 0.5f);
    REQUIRE_THAT(mid.r, WithinAbs(0.5f, 1e-5));
    REQUIRE_THAT(mid.a, WithinAbs(0.5f, 1e-5));

    Color start = Color::Lerp(a, b, 0.0f);
    REQUIRE(start.r == 0.0f);

    Color end = Color::Lerp(a, b, 1.0f);
    REQUIRE(end.r == 1.0f);
}

TEST_CASE("Color HSV roundtrip", "[color]") {
    // Red
    Color red = Color::FromHSV(0.0f, 1.0f, 1.0f);
    REQUIRE_THAT(red.r, WithinAbs(1.0f, 1e-4));
    REQUIRE_THAT(red.g, WithinAbs(0.0f, 1e-4));
    REQUIRE_THAT(red.b, WithinAbs(0.0f, 1e-4));

    float h, s, v;
    red.ToHSV(h, s, v);
    REQUIRE_THAT(s, WithinAbs(1.0f, 1e-4));
    REQUIRE_THAT(v, WithinAbs(1.0f, 1e-4));

    // Green
    Color green = Color::FromHSV(120.0f, 1.0f, 1.0f);
    REQUIRE_THAT(green.r, WithinAbs(0.0f, 1e-4));
    REQUIRE_THAT(green.g, WithinAbs(1.0f, 1e-4));

    // Blue
    Color blue = Color::FromHSV(240.0f, 1.0f, 1.0f);
    REQUIRE_THAT(blue.b, WithinAbs(1.0f, 1e-4));

    // Roundtrip arbitrary color
    Color orig(0.6f, 0.3f, 0.8f, 1.0f);
    orig.ToHSV(h, s, v);
    Color back = Color::FromHSV(h, s, v);
    REQUIRE_THAT(back.r, WithinAbs(orig.r, 1e-3));
    REQUIRE_THAT(back.g, WithinAbs(orig.g, 1e-3));
    REQUIRE_THAT(back.b, WithinAbs(orig.b, 1e-3));
}

TEST_CASE("Color FromHex", "[color]") {
    Color white = Color::FromHex("#FFFFFF");
    REQUIRE_THAT(white.r, WithinAbs(1.0f, 1e-3));
    REQUIRE_THAT(white.g, WithinAbs(1.0f, 1e-3));
    REQUIRE_THAT(white.b, WithinAbs(1.0f, 1e-3));
    REQUIRE(white.a == 1.0f);

    Color red = Color::FromHex("#FF0000");
    REQUIRE_THAT(red.r, WithinAbs(1.0f, 1e-3));
    REQUIRE_THAT(red.g, WithinAbs(0.0f, 1e-3));

    Color withAlpha = Color::FromHex("#FF000080");
    REQUIRE_THAT(withAlpha.r, WithinAbs(1.0f, 1e-3));
    REQUIRE_THAT(withAlpha.a, WithinAbs(128.0f / 255.0f, 1e-3));

    Color invalid = Color::FromHex("not-a-hex");
    REQUIRE(invalid.r == 0.0f);
}

TEST_CASE("Color equality", "[color]") {
    REQUIRE(Color(1, 0, 0, 1) == Color(1, 0, 0, 1));
    REQUIRE(Color(1, 0, 0, 1) != Color(0, 1, 0, 1));
}
