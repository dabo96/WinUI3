#pragma once
#include <ft2build.h>
#include FT_FREETYPE_H
#include "Math/Vec2.h"
#include <vector>
#include <cstdint>
#include <memory>
#include <cmath>

namespace FluentUI {

    // Structure to hold MSDF data
    struct MSDFData {
        std::vector<unsigned char> pixels; // RGB MSDF data
        int width;
        int height;
        float pixelRange;

        MSDFData(int w, int h, float range)
            : width(std::max(w, 1)), height(std::max(h, 1)), pixelRange(range) {
            pixels.resize(static_cast<size_t>(width) * height * 3, 0);
        }
    };

    class MSDFGenerator {
    public:
        MSDFGenerator();
        ~MSDFGenerator();

        // Generate MSDF from FreeType outline
        // Returns MSDF data or nullptr on failure
        std::unique_ptr<MSDFData> GenerateFromOutline(
            FT_Outline* outline,
            int width,
            int height,
            float pixelRange = 4.0f
        );

        // Generate MSDF for a glyph from FreeType face
        std::unique_ptr<MSDFData> GenerateFromGlyph(
            FT_Face face,
            uint32_t glyphIndex,
            int size,
            float pixelRange = 4.0f,
            int padding = 4
        );

    private:
        struct Point {
            float x, y;
            Point(float x = 0, float y = 0) : x(x), y(y) {}
            Point operator+(const Point& p) const { return Point(x + p.x, y + p.y); }
            Point operator-(const Point& p) const { return Point(x - p.x, y - p.y); }
            Point operator*(float s) const { return Point(x * s, y * s); }
            float dot(const Point& p) const { return x * p.x + y * p.y; }
            float cross(const Point& p) const { return x * p.y - y * p.x; }
            float length() const { return std::sqrt(x * x + y * y); }
            float lengthSq() const { return x * x + y * y; }
        };

        struct Edge {
            Point p0, p1, p2; // For line: p0->p1. For quadratic: p0->p2 with control p1
            bool isQuadratic;
            uint8_t colorMask; // Bitmask: bit0=R, bit1=G, bit2=B

            Edge(const Point& a, const Point& b, const Point& c = Point(), bool quad = false)
                : p0(a), p1(b), p2(c), isQuadratic(quad), colorMask(0x7) {}
        };

        struct Contour {
            std::vector<Edge> edges;
        };

        // Process FreeType outline into per-contour edge lists
        void ProcessOutline(FT_Outline* outline, std::vector<Contour>& contours);

        // Assign color channels to edges within each contour
        void ColorContourEdges(std::vector<Contour>& contours);

        // Calculate signed distance from point to edge (uses tangent for sign)
        float SignedDistance(const Point& p, const Edge& edge) const;

        // Calculate MSDF value at a point using per-channel closest distances
        void CalculateMSDF(MSDFData* msdf, const std::vector<Contour>& contours, int x, int y);

        // Transform contours to texture space and generate MSDF
        void FillMSDF(MSDFData* msdf, std::vector<Contour>& contours);
    };

} // namespace FluentUI

