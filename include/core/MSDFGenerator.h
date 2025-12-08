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
            : width(w), height(h), pixelRange(range) {
            pixels.resize(w * h * 3);
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
        // Helper structures for contour processing
        struct Point {
            float x, y;
            Point(float x = 0, float y = 0) : x(x), y(y) {}
            Point operator+(const Point& p) const { return Point(x + p.x, y + p.y); }
            Point operator-(const Point& p) const { return Point(x - p.x, y - p.y); }
            Point operator*(float s) const { return Point(x * s, y * s); }
            float dot(const Point& p) const { return x * p.x + y * p.y; }
            float length() const { 
                using std::sqrt;
                return sqrt(x * x + y * y); 
            }
            void normalize() {
                float len = length();
                if (len > 0.0f) {
                    x /= len;
                    y /= len;
                }
            }
        };

        struct Edge {
            Point p0, p1, p2; // For quadratic: p0->p2 with control p1
            bool isQuadratic;
            
            Edge(const Point& a, const Point& b, const Point& c = Point(), bool quad = false)
                : p0(a), p1(b), p2(c), isQuadratic(quad) {}
        };

        // Process FreeType outline into edges
        void ProcessOutline(FT_Outline* outline, std::vector<Edge>& edges);

        // Calculate signed distance from point to edge
        float SignedDistance(const Point& p, const Edge& edge) const;

        // Calculate MSDF value at a point
        void CalculateMSDF(MSDFData* msdf, const std::vector<Edge>& edges, int x, int y);

        // Fill MSDF data
        void FillMSDF(MSDFData* msdf, const std::vector<Edge>& edges);
    };

} // namespace FluentUI

