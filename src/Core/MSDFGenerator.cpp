#include "core/MSDFGenerator.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace FluentUI {

    MSDFGenerator::MSDFGenerator() = default;
    MSDFGenerator::~MSDFGenerator() = default;

    std::unique_ptr<MSDFData> MSDFGenerator::GenerateFromOutline(
        FT_Outline* outline,
        int width,
        int height,
        float pixelRange) {
        
        if (!outline || width <= 0 || height <= 0) {
            return nullptr;
        }

        auto msdf = std::make_unique<MSDFData>(width, height, pixelRange);

        // Process outline into edges
        std::vector<Edge> edges;
        ProcessOutline(outline, edges);

        // Fill MSDF
        FillMSDF(msdf.get(), edges);

        return msdf;
    }

    std::unique_ptr<MSDFData> MSDFGenerator::GenerateFromGlyph(
        FT_Face face,
        uint32_t glyphIndex,
        int size,
        float pixelRange,
        int padding) {
        
        if (!face) {
            return nullptr;
        }

        // Load glyph
        if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP)) {
            return nullptr;
        }

        FT_Outline* outline = &face->glyph->outline;
        if (!outline) {
            return nullptr;
        }

        // Calculate bounding box manually (FT_Outline_Get_BBox doesn't exist in standard FreeType)
        FT_BBox bbox;
        bbox.xMin = bbox.yMin = 32000;
        bbox.xMax = bbox.yMax = -32000;
        
        for (int i = 0; i < outline->n_points; ++i) {
            FT_Vector* point = &outline->points[i];
            if (point->x < bbox.xMin) bbox.xMin = point->x;
            if (point->x > bbox.xMax) bbox.xMax = point->x;
            if (point->y < bbox.yMin) bbox.yMin = point->y;
            if (point->y > bbox.yMax) bbox.yMax = point->y;
        }

        // Add padding and calculate size
        int width = static_cast<int>((bbox.xMax - bbox.xMin) / 64.0f) + padding * 2;
        int height = static_cast<int>((bbox.yMax - bbox.yMin) / 64.0f) + padding * 2;

        // Ensure minimum size
        width = std::max(width, size);
        height = std::max(height, size);

        return GenerateFromOutline(outline, width, height, pixelRange);
    }

    void MSDFGenerator::ProcessOutline(FT_Outline* outline, std::vector<Edge>& edges) {
        if (!outline) return;

        int start = 0;
        Point firstPoint;

        for (int i = 0; i < outline->n_contours; ++i) {
            int end = outline->contours[i];
            Point contourStart;

            for (int j = start; j <= end; ++j) {
                FT_Vector point = outline->points[j];
                Point p(point.x / 64.0f, point.y / 64.0f);

                char tag = outline->tags[j];
                bool onCurve = (tag & 1) != 0;

                if (j == start) {
                    contourStart = p;
                    firstPoint = p;
                    continue;
                }

                // Previous point
                FT_Vector prevPoint = outline->points[j - 1];
                Point prev(prevPoint.x / 64.0f, prevPoint.y / 64.0f);
                char prevTag = outline->tags[j - 1];
                bool prevOnCurve = (prevTag & 1) != 0;

                if (onCurve) {
                    if (prevOnCurve) {
                        // Straight line
                        edges.emplace_back(prev, p, Point(), false);
                    } else {
                        // End of quadratic curve
                        // The previous point was a control point
                        // The point before that (or start) is the start point
                        Point startPoint = prev;
                        if (j - 1 > start) {
                            FT_Vector beforePrev = outline->points[j - 2];
                            startPoint = Point(beforePrev.x / 64.0f, beforePrev.y / 64.0f);
                        } else {
                            startPoint = contourStart;
                        }
                        edges.emplace_back(startPoint, prev, p, true);
                    }
                } else {
                    // This is a control point
                    // Next point should be on curve (or end of contour)
                    if (j < end) {
                        FT_Vector nextPoint = outline->points[j + 1];
                        Point next(nextPoint.x / 64.0f, nextPoint.y / 64.0f);
                        char nextTag = outline->tags[j + 1];
                        bool nextOnCurve = (nextTag & 1) != 0;

                        if (nextOnCurve) {
                            // Quadratic curve: prev -> next with control p
                            edges.emplace_back(prev, p, next, true);
                        } else {
                            // Cubic curve: prev -> (p + next)/2 with control points
                            // For simplicity, we'll approximate as quadratic
                            Point mid((p.x + next.x) * 0.5f, (p.y + next.y) * 0.5f);
                            edges.emplace_back(prev, p, mid, true);
                        }
                    }
                }

                // Close contour if needed
                if (j == end && i < outline->n_contours - 1) {
                    if (onCurve) {
                        edges.emplace_back(p, firstPoint, Point(), false);
                    }
                }
            }

            start = end + 1;
        }
    }

    float MSDFGenerator::SignedDistance(const Point& p, const Edge& edge) const {
        if (edge.isQuadratic) {
            // Quadratic Bézier curve
            // Distance to quadratic curve is more complex
            // Simplified approach: sample the curve and find minimum distance
            float minDist = 1e10f;
            const int samples = 20;
            
            for (int i = 0; i <= samples; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(samples);
                Point curvePoint = edge.p0 * (1.0f - t) * (1.0f - t) +
                                   edge.p1 * 2.0f * (1.0f - t) * t +
                                   edge.p2 * t * t;
                
                Point diff = p - curvePoint;
                float dist = diff.length();
                minDist = std::min(minDist, dist);
            }

            // Determine sign (inside/outside) using winding number
            // Simplified: check if point is to the left of the curve
            Point dir = edge.p2 - edge.p0;
            Point toPoint = p - edge.p0;
            float cross = dir.x * toPoint.y - dir.y * toPoint.x;
            
            return (cross > 0.0f) ? minDist : -minDist;
        } else {
            // Straight line
            Point dir = edge.p1 - edge.p0;
            Point toPoint = p - edge.p0;
            
            float t = std::max(0.0f, std::min(1.0f, toPoint.dot(dir) / dir.dot(dir)));
            Point closest = edge.p0 + dir * t;
            
            Point diff = p - closest;
            float dist = diff.length();
            
            // Determine sign using cross product
            float cross = dir.x * diff.y - dir.y * diff.x;
            return (cross > 0.0f) ? dist : -dist;
        }
    }

    void MSDFGenerator::CalculateMSDF(MSDFData* msdf, const std::vector<Edge>& edges, int x, int y) {
        Point p(static_cast<float>(x), static_cast<float>(y));

        // Find closest edges for each channel (R, G, B)
        // For simplicity, we'll use the three closest edges
        struct EdgeDist {
            float distance;
            size_t edgeIndex;
        };
        
        std::vector<EdgeDist> distances;
        for (size_t i = 0; i < edges.size(); ++i) {
            float dist = SignedDistance(p, edges[i]);
            distances.push_back({std::abs(dist), i});
        }

        // Sort by distance
        std::sort(distances.begin(), distances.end(),
                  [](const EdgeDist& a, const EdgeDist& b) {
                      return a.distance < b.distance;
                  });

        // Get distances for three channels
        float d1 = SignedDistance(p, edges[distances[0].edgeIndex]);
        float d2 = distances.size() > 1 ? SignedDistance(p, edges[distances[1].edgeIndex]) : d1;
        float d3 = distances.size() > 2 ? SignedDistance(p, edges[distances[2].edgeIndex]) : d1;

        // Convert to MSDF format (normalize to [0, 1] range)
        // MSDF stores: 0.5 at edge, >0.5 inside, <0.5 outside
        float range = msdf->pixelRange;
        float r = std::max(0.0f, std::min(1.0f, 0.5f + d1 / range));
        float g = std::max(0.0f, std::min(1.0f, 0.5f + d2 / range));
        float b = std::max(0.0f, std::min(1.0f, 0.5f + d3 / range));

        int index = (y * msdf->width + x) * 3;
        msdf->pixels[index] = static_cast<unsigned char>(r * 255.0f);
        msdf->pixels[index + 1] = static_cast<unsigned char>(g * 255.0f);
        msdf->pixels[index + 2] = static_cast<unsigned char>(b * 255.0f);
    }

    void MSDFGenerator::FillMSDF(MSDFData* msdf, const std::vector<Edge>& edges) {
        if (!msdf || edges.empty()) return;

        // Transform coordinates to match MSDF texture space
        // We need to find the bounding box and adjust coordinates
        float minX = 1e10f, minY = 1e10f, maxX = -1e10f, maxY = -1e10f;
        
        for (const auto& edge : edges) {
            minX = std::min({minX, edge.p0.x, edge.p1.x, edge.p2.x});
            minY = std::min({minY, edge.p0.y, edge.p1.y, edge.p2.y});
            maxX = std::max({maxX, edge.p0.x, edge.p1.x, edge.p2.x});
            maxY = std::max({maxY, edge.p0.y, edge.p1.y, edge.p2.y});
        }

        // Center and scale
        float centerX = (minX + maxX) * 0.5f;
        float centerY = (minY + maxY) * 0.5f;
        float scaleX = static_cast<float>(msdf->width - 1) / std::max(1.0f, maxX - minX);
        float scaleY = static_cast<float>(msdf->height - 1) / std::max(1.0f, maxY - minY);
        float scale = std::min(scaleX, scaleY) * 0.9f; // Slight padding

        // Transform edges
        std::vector<Edge> transformedEdges;
        for (const auto& edge : edges) {
            Point p0 = (edge.p0 - Point(centerX, centerY)) * scale + Point(msdf->width * 0.5f, msdf->height * 0.5f);
            Point p1 = (edge.p1 - Point(centerX, centerY)) * scale + Point(msdf->width * 0.5f, msdf->height * 0.5f);
            Point p2 = (edge.p2 - Point(centerX, centerY)) * scale + Point(msdf->width * 0.5f, msdf->height * 0.5f);
            transformedEdges.emplace_back(p0, p1, p2, edge.isQuadratic);
        }

        // Generate MSDF for each pixel
        for (int y = 0; y < msdf->height; ++y) {
            for (int x = 0; x < msdf->width; ++x) {
                CalculateMSDF(msdf, transformedEdges, x, y);
            }
        }
    }

} // namespace FluentUI

