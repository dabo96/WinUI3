#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Theme/Material.h"

using namespace FluentUI;
using Catch::Matchers::WithinAbs;

static FluentMaterial SampleMaterial() {
    FluentMaterial m;
    m.fill              = Color(0.10f, 0.20f, 0.30f, 1.00f);
    m.border            = Color(0.50f, 0.60f, 0.70f, 0.80f);
    m.borderWidth       = 1.0f;
    m.radius            = 12.0f;
    m.elevationZ        = 4.0f;
    m.revealIntensity   = 1.0f;
    m.acrylic           = true;
    m.tintOpacity       = 0.25f;
    m.luminosityOpacity = 0.90f;
    return m;
}

TEST_CASE("Material JSON round-trip preserves fields", "[material][json]") {
    FluentMaterial in = SampleMaterial();
    FluentMaterial out = MaterialFromJson(MaterialToJson(in));

    // Colors survive the 8-bit hex quantization to within half a step.
    REQUIRE_THAT(out.fill.r,   WithinAbs(in.fill.r,   1.0f / 255.0f));
    REQUIRE_THAT(out.fill.a,   WithinAbs(in.fill.a,   1.0f / 255.0f));
    REQUIRE_THAT(out.border.b, WithinAbs(in.border.b, 1.0f / 255.0f));
    REQUIRE_THAT(out.border.a, WithinAbs(in.border.a, 1.0f / 255.0f));

    REQUIRE_THAT(out.borderWidth,       WithinAbs(in.borderWidth,       1e-4f));
    REQUIRE_THAT(out.radius,            WithinAbs(in.radius,            1e-4f));
    REQUIRE_THAT(out.elevationZ,        WithinAbs(in.elevationZ,        1e-4f));
    REQUIRE_THAT(out.revealIntensity,   WithinAbs(in.revealIntensity,   1e-4f));
    REQUIRE_THAT(out.tintOpacity,       WithinAbs(in.tintOpacity,       1e-4f));
    REQUIRE_THAT(out.luminosityOpacity, WithinAbs(in.luminosityOpacity, 1e-4f));
    REQUIRE(out.acrylic == in.acrylic);
}

TEST_CASE("Material JSON tolerates missing keys (keeps defaults)", "[material][json]") {
    FluentMaterial def; // defaults
    FluentMaterial out = MaterialFromJson("{ \"radius\": 20.0 }");
    REQUIRE_THAT(out.radius, WithinAbs(20.0f, 1e-4f));
    // Untouched fields stay at the struct default.
    REQUIRE_THAT(out.tintOpacity, WithinAbs(def.tintOpacity, 1e-4f));
    REQUIRE(out.acrylic == def.acrylic);
}

TEST_CASE("Material JSON tolerates unknown keys", "[material][json]") {
    FluentMaterial out = MaterialFromJson(
        "{ \"radius\": 6.0, \"futureField\": 99.0, \"acrylic\": true }");
    REQUIRE_THAT(out.radius, WithinAbs(6.0f, 1e-4f));
    REQUIRE(out.acrylic == true);
}

TEST_CASE("Material JSON emits hex color strings", "[material][json]") {
    FluentMaterial m;
    m.fill = Color(1.0f, 0.0f, 0.0f, 1.0f);
    std::string json = MaterialToJson(m);
    REQUIRE(json.find("#FF0000FF") != std::string::npos);
}
