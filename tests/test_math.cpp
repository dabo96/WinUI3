#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Math/Vec2.h"
#include "Math/Rect.h"

using namespace FluentUI;
using Catch::Matchers::WithinAbs;

// ============================================================================
// Vec2 Tests
// ============================================================================

TEST_CASE("Vec2 default constructor", "[vec2]") {
    Vec2 v;
    REQUIRE(v.x == 0.0f);
    REQUIRE(v.y == 0.0f);
}

TEST_CASE("Vec2 parameterized constructor", "[vec2]") {
    Vec2 v(3.0f, 4.0f);
    REQUIRE(v.x == 3.0f);
    REQUIRE(v.y == 4.0f);
}

TEST_CASE("Vec2 arithmetic operators", "[vec2]") {
    Vec2 a(1.0f, 2.0f);
    Vec2 b(3.0f, 4.0f);

    SECTION("addition") {
        Vec2 c = a + b;
        REQUIRE(c.x == 4.0f);
        REQUIRE(c.y == 6.0f);
    }

    SECTION("subtraction") {
        Vec2 c = b - a;
        REQUIRE(c.x == 2.0f);
        REQUIRE(c.y == 2.0f);
    }

    SECTION("scalar multiply") {
        Vec2 c = a * 2.0f;
        REQUIRE(c.x == 2.0f);
        REQUIRE(c.y == 4.0f);
    }

    SECTION("scalar divide") {
        Vec2 c = b / 2.0f;
        REQUIRE(c.x == 1.5f);
        REQUIRE(c.y == 2.0f);
    }

    SECTION("component-wise multiply") {
        Vec2 c = a * b;
        REQUIRE(c.x == 3.0f);
        REQUIRE(c.y == 8.0f);
    }

    SECTION("negation") {
        Vec2 c = -a;
        REQUIRE(c.x == -1.0f);
        REQUIRE(c.y == -2.0f);
    }

    SECTION("compound operators") {
        Vec2 c = a;
        c += b;
        REQUIRE(c.x == 4.0f);
        c -= a;
        REQUIRE(c.x == 3.0f);
        c *= 2.0f;
        REQUIRE(c.x == 6.0f);
        c /= 3.0f;
        REQUIRE(c.x == 2.0f);
    }
}

TEST_CASE("Vec2 vector operations", "[vec2]") {
    Vec2 a(3.0f, 4.0f);

    SECTION("length") {
        REQUIRE_THAT(a.length(), WithinAbs(5.0f, 1e-5));
    }

    SECTION("lengthSquared") {
        REQUIRE(a.lengthSquared() == 25.0f);
    }

    SECTION("dot product") {
        Vec2 b(1.0f, 0.0f);
        REQUIRE(a.dot(b) == 3.0f);
    }

    SECTION("cross product") {
        Vec2 b(0.0f, 1.0f);
        Vec2 c(1.0f, 0.0f);
        REQUIRE(c.cross(b) == 1.0f);
    }

    SECTION("normalized") {
        Vec2 n = a.normalized();
        REQUIRE_THAT(n.length(), WithinAbs(1.0f, 1e-5));
        REQUIRE_THAT(n.x, WithinAbs(0.6f, 1e-5));
        REQUIRE_THAT(n.y, WithinAbs(0.8f, 1e-5));
    }

    SECTION("normalized zero vector") {
        Vec2 zero;
        Vec2 n = zero.normalized();
        REQUIRE(n.x == 0.0f);
        REQUIRE(n.y == 0.0f);
    }

    SECTION("perp") {
        Vec2 p = a.perp();
        REQUIRE(p.x == -4.0f);
        REQUIRE(p.y == 3.0f);
        REQUIRE_THAT(a.dot(p), WithinAbs(0.0f, 1e-5));
    }

    SECTION("distance") {
        Vec2 b(0.0f, 0.0f);
        REQUIRE_THAT(a.distance(b), WithinAbs(5.0f, 1e-5));
    }

    SECTION("clamp") {
        Vec2 v(10.0f, -5.0f);
        Vec2 c = v.clamp(Vec2(0, 0), Vec2(5, 5));
        REQUIRE(c.x == 5.0f);
        REQUIRE(c.y == 0.0f);
    }
}

TEST_CASE("Vec2 static methods", "[vec2]") {
    SECTION("Lerp") {
        Vec2 a(0, 0), b(10, 20);
        Vec2 mid = Vec2::Lerp(a, b, 0.5f);
        REQUIRE(mid.x == 5.0f);
        REQUIRE(mid.y == 10.0f);
    }

    SECTION("Min/Max") {
        Vec2 a(1, 5), b(3, 2);
        Vec2 mn = Vec2::Min(a, b);
        Vec2 mx = Vec2::Max(a, b);
        REQUIRE(mn.x == 1.0f);
        REQUIRE(mn.y == 2.0f);
        REQUIRE(mx.x == 3.0f);
        REQUIRE(mx.y == 5.0f);
    }
}

TEST_CASE("Vec2 equality", "[vec2]") {
    REQUIRE(Vec2(1, 2) == Vec2(1, 2));
    REQUIRE(Vec2(1, 2) != Vec2(1, 3));
}

// ============================================================================
// Rect Tests
// ============================================================================

TEST_CASE("Rect default constructor", "[rect]") {
    Rect r;
    REQUIRE(r.pos == Vec2(0, 0));
    REQUIRE(r.size == Vec2(0, 0));
    REQUIRE(r.IsEmpty());
}

TEST_CASE("Rect accessors", "[rect]") {
    Rect r(10, 20, 100, 50);
    REQUIRE(r.Left() == 10.0f);
    REQUIRE(r.Top() == 20.0f);
    REQUIRE(r.Right() == 110.0f);
    REQUIRE(r.Bottom() == 70.0f);
    REQUIRE(r.Width() == 100.0f);
    REQUIRE(r.Height() == 50.0f);
    REQUIRE_THAT(r.Center().x, WithinAbs(60.0f, 1e-5));
    REQUIRE_THAT(r.Center().y, WithinAbs(45.0f, 1e-5));
}

TEST_CASE("Rect Contains", "[rect]") {
    Rect r(0, 0, 100, 100);
    REQUIRE(r.Contains(Vec2(50, 50)));
    REQUIRE(r.Contains(Vec2(0, 0)));
    REQUIRE(r.Contains(Vec2(100, 100)));
    REQUIRE_FALSE(r.Contains(Vec2(-1, 50)));
    REQUIRE_FALSE(r.Contains(Vec2(101, 50)));
}

TEST_CASE("Rect Overlaps", "[rect]") {
    Rect a(0, 0, 100, 100);
    Rect b(50, 50, 100, 100);
    Rect c(200, 200, 10, 10);

    REQUIRE(a.Overlaps(b));
    REQUIRE(b.Overlaps(a));
    REQUIRE_FALSE(a.Overlaps(c));
}

TEST_CASE("Rect Intersection", "[rect]") {
    Rect a(0, 0, 100, 100);
    Rect b(50, 50, 100, 100);

    Rect inter = a.Intersection(b);
    REQUIRE(inter.Left() == 50.0f);
    REQUIRE(inter.Top() == 50.0f);
    REQUIRE(inter.Right() == 100.0f);
    REQUIRE(inter.Bottom() == 100.0f);

    Rect noOverlap = a.Intersection(Rect(200, 200, 10, 10));
    REQUIRE(noOverlap.IsEmpty());
}

TEST_CASE("Rect Union", "[rect]") {
    Rect a(10, 20, 30, 40);
    Rect b(50, 60, 70, 80);
    Rect u = a.Union(b);
    REQUIRE(u.Left() == 10.0f);
    REQUIRE(u.Top() == 20.0f);
    REQUIRE(u.Right() == 120.0f);
    REQUIRE(u.Bottom() == 140.0f);
}

TEST_CASE("Rect Expanded and Shrunk", "[rect]") {
    Rect r(10, 10, 100, 100);
    Rect exp = r.Expanded(5);
    REQUIRE(exp.Left() == 5.0f);
    REQUIRE(exp.Width() == 110.0f);

    Rect shr = r.Shrunk(5);
    REQUIRE(shr.Left() == 15.0f);
    REQUIRE(shr.Width() == 90.0f);
}
