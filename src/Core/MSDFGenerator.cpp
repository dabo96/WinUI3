#include "core/MSDFGenerator.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <limits>

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

        std::vector<Contour> contours;
        ProcessOutline(outline, contours);

        if (contours.empty()) {
            return msdf;
        }

        FillMSDF(msdf.get(), contours);

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

        if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP)) {
            return nullptr;
        }

        FT_Outline* outline = &face->glyph->outline;
        if (!outline || outline->n_points <= 0) {
            return nullptr;
        }

        // Calculate bounding box
        FT_BBox bbox;
        bbox.xMin = bbox.yMin = std::numeric_limits<long>::max();
        bbox.xMax = bbox.yMax = std::numeric_limits<long>::min();

        for (int i = 0; i < outline->n_points; ++i) {
            FT_Vector* point = &outline->points[i];
            if (point->x < bbox.xMin) bbox.xMin = point->x;
            if (point->x > bbox.xMax) bbox.xMax = point->x;
            if (point->y < bbox.yMin) bbox.yMin = point->y;
            if (point->y > bbox.yMax) bbox.yMax = point->y;
        }

        int width = static_cast<int>((bbox.xMax - bbox.xMin) / 64.0f) + padding * 2;
        int height = static_cast<int>((bbox.yMax - bbox.yMin) / 64.0f) + padding * 2;

        width = std::max(width, size);
        height = std::max(height, size);

        return GenerateFromOutline(outline, width, height, pixelRange);
    }

    void MSDFGenerator::ProcessOutline(FT_Outline* outline, std::vector<Contour>& contours) {
        if (!outline) return;

        int start = 0;

        for (int i = 0; i < outline->n_contours; ++i) {
            int end = outline->contours[i];

            // Phase 1: Collect points, inserting implicit on-curve midpoints
            // between consecutive off-curve points (TrueType convention)
            std::vector<Point> points;
            std::vector<bool> onCurve;

            for (int j = start; j <= end; ++j) {
                FT_Vector pt = outline->points[j];
                Point p(pt.x / 64.0f, pt.y / 64.0f);
                bool isOn = (outline->tags[j] & 1) != 0;

                if (!points.empty() && !onCurve.back() && !isOn) {
                    // Two consecutive off-curve: insert implicit on-curve midpoint
                    Point prev = points.back();
                    points.push_back(Point((prev.x + p.x) * 0.5f, (prev.y + p.y) * 0.5f));
                    onCurve.push_back(true);
                }

                points.push_back(p);
                onCurve.push_back(isOn);
            }

            if (points.size() < 2) {
                start = end + 1;
                continue;
            }

            // Handle wrap-around: last off-curve + first off-curve
            if (!onCurve.back() && !onCurve.front()) {
                Point mid((points.back().x + points.front().x) * 0.5f,
                          (points.back().y + points.front().y) * 0.5f);
                points.push_back(mid);
                onCurve.push_back(true);
            }

            int n = static_cast<int>(points.size());

            // Find first on-curve point
            int firstOn = -1;
            for (int j = 0; j < n; ++j) {
                if (onCurve[j]) { firstOn = j; break; }
            }

            if (firstOn < 0) {
                // Shouldn't happen after midpoint insertion, but handle gracefully
                Point mid((points[0].x + points[1].x) * 0.5f,
                          (points[0].y + points[1].y) * 0.5f);
                points.insert(points.begin(), mid);
                onCurve.insert(onCurve.begin(), true);
                firstOn = 0;
                n = static_cast<int>(points.size());
            }

            // Phase 2: Generate edges by walking from firstOn
            Contour contour;
            int idx = firstOn;
            int maxIter = n + 1;
            int iter = 0;

            do {
                if (++iter > maxIter) break;

                int next = (idx + 1) % n;

                if (onCurve[next]) {
                    // Line segment
                    contour.edges.emplace_back(points[idx], points[next], Point(), false);
                    idx = next;
                } else {
                    // Quadratic bezier: current on-curve -> control -> next on-curve
                    int afterControl = (next + 1) % n;
                    contour.edges.emplace_back(points[idx], points[next], points[afterControl], true);
                    idx = afterControl;
                }
            } while (idx != firstOn);

            if (!contour.edges.empty()) {
                contours.push_back(std::move(contour));
            }

            start = end + 1;
        }
    }

    void MSDFGenerator::ColorContourEdges(std::vector<Contour>& contours) {
        // Assign color channels per contour so that adjacent edges
        // at corners have different channel combinations.
        // Uses CMY coloring: Cyan={GB}, Magenta={RB}, Yellow={RG}
        // Adjacent edges share exactly one channel and differ in another,
        // allowing the median operation to detect sharp corners.

        for (auto& contour : contours) {
            int n = static_cast<int>(contour.edges.size());
            if (n == 0) continue;

            if (n == 1) {
                // Single edge: assign all channels (acts like SDF)
                contour.edges[0].colorMask = 0x7; // RGB
            } else if (n == 2) {
                // Two edges: use two CMY colors that share one channel
                contour.edges[0].colorMask = 0x3; // RG (Yellow)
                contour.edges[1].colorMask = 0x6; // GB (Cyan)
            } else {
                // 3+ edges: rotate through CMY
                // Yellow(RG=0x3), Cyan(GB=0x6), Magenta(RB=0x5)
                static const uint8_t cmyColors[3] = { 0x3, 0x6, 0x5 };
                for (int i = 0; i < n; ++i) {
                    contour.edges[i].colorMask = cmyColors[i % 3];
                }
            }
        }
    }

    // Solve cubic at^3 + bt^2 + ct + d = 0, return real roots in [0,1]
    static int SolveCubicRoots(float a, float b, float c, float d, float roots[3]) {
        int count = 0;
        const float eps = 1e-6f;

        if (std::abs(a) < eps) {
            if (std::abs(b) < eps) {
                if (std::abs(c) > eps) {
                    float r = -d / c;
                    if (r >= 0.0f && r <= 1.0f) roots[count++] = r;
                }
            } else {
                float disc = c * c - 4.0f * b * d;
                if (disc >= 0.0f) {
                    float sq = std::sqrt(disc);
                    float r1 = (-c + sq) / (2.0f * b);
                    float r2 = (-c - sq) / (2.0f * b);
                    if (r1 >= 0.0f && r1 <= 1.0f) roots[count++] = r1;
                    if (r2 >= 0.0f && r2 <= 1.0f && std::abs(r2 - r1) > eps) roots[count++] = r2;
                }
            }
            return count;
        }

        float inv_a = 1.0f / a;
        float B = b * inv_a;
        float C = c * inv_a;
        float D = d * inv_a;

        float p = C - B * B / 3.0f;
        float q = 2.0f * B * B * B / 27.0f - B * C / 3.0f + D;

        float disc = q * q / 4.0f + p * p * p / 27.0f;
        float offset = -B / 3.0f;

        if (disc > eps) {
            float sqrtDisc = std::sqrt(disc);
            float u = std::cbrt(-q / 2.0f + sqrtDisc);
            float v = std::cbrt(-q / 2.0f - sqrtDisc);
            float r = u + v + offset;
            if (r >= 0.0f && r <= 1.0f) roots[count++] = r;
        } else if (disc < -eps) {
            float m = 2.0f * std::sqrt(-p / 3.0f);
            float theta = std::acos(std::max(-1.0f, std::min(1.0f, 3.0f * q / (p * m)))) / 3.0f;
            constexpr float pi = 3.14159265358979323846f;
            for (int k = 0; k < 3; ++k) {
                float r = m * std::cos(theta - 2.0f * pi * k / 3.0f) + offset;
                if (r >= -eps && r <= 1.0f + eps) {
                    roots[count++] = std::max(0.0f, std::min(1.0f, r));
                }
            }
        } else {
            float u = std::cbrt(-q / 2.0f);
            float r1 = 2.0f * u + offset;
            float r2 = -u + offset;
            if (r1 >= 0.0f && r1 <= 1.0f) roots[count++] = r1;
            if (r2 >= 0.0f && r2 <= 1.0f && std::abs(r2 - r1) > eps) roots[count++] = r2;
        }
        return count;
    }

    float MSDFGenerator::SignedDistance(const Point& p, const Edge& edge) const {
        if (edge.isQuadratic) {
            // Quadratic Bezier B(t) = (1-t)^2*P0 + 2t(1-t)*P1 + t^2*P2
            // Minimize |B(t) - p|^2 => derivative is cubic in t
            Point a = edge.p0 - edge.p1 * 2.0f + edge.p2;
            Point b2 = (edge.p1 - edge.p0) * 2.0f;
            Point c2 = edge.p0 - p;

            float A3 = 2.0f * a.dot(a);
            float A2 = 3.0f * a.dot(b2);
            float A1 = b2.dot(b2) + 2.0f * a.dot(c2);
            float A0 = b2.dot(c2);

            float roots[3];
            int nRoots = SolveCubicRoots(A3, A2, A1, A0, roots);

            float minDistSq = 1e10f;
            float bestT = 0.0f;

            auto evalBezier = [&](float t) -> Point {
                float omt = 1.0f - t;
                return edge.p0 * (omt * omt) + edge.p1 * (2.0f * omt * t) + edge.p2 * (t * t);
            };

            auto checkT = [&](float t) {
                Point bp = evalBezier(t);
                float distSq = (p - bp).lengthSq();
                if (distSq < minDistSq) {
                    minDistSq = distSq;
                    bestT = t;
                }
            };

            checkT(0.0f);
            checkT(1.0f);
            for (int i = 0; i < nRoots; ++i) {
                checkT(roots[i]);
            }

            float minDist = std::sqrt(minDistSq);

            // Compute tangent at closest point for sign determination
            // B'(t) = 2[(1-t)(P1-P0) + t(P2-P1)]
            Point tangent = (edge.p1 - edge.p0) * (2.0f * (1.0f - bestT)) +
                            (edge.p2 - edge.p1) * (2.0f * bestT);

            // Fallback to chord direction if tangent is degenerate
            if (tangent.lengthSq() < 1e-12f) {
                tangent = edge.p2 - edge.p0;
            }

            // Sign from cross product: tangent x (p - closest)
            Point closest = evalBezier(bestT);
            Point diff = p - closest;
            float cross = tangent.cross(diff);

            return (cross >= 0.0f) ? minDist : -minDist;
        } else {
            // Straight line
            Point dir = edge.p1 - edge.p0;
            Point toPoint = p - edge.p0;

            float lenSq = dir.dot(dir);
            float t = (lenSq > 0.0f) ? std::max(0.0f, std::min(1.0f, toPoint.dot(dir) / lenSq)) : 0.0f;
            Point closest = edge.p0 + dir * t;

            Point diff = p - closest;
            float dist = diff.length();

            float cross = dir.cross(diff);
            return (cross >= 0.0f) ? dist : -dist;
        }
    }

    void MSDFGenerator::CalculateMSDF(MSDFData* msdf, const std::vector<Contour>& contours, int x, int y) {
        Point p(static_cast<float>(x), static_cast<float>(y));

        // Per-channel: track the closest edge's signed distance
        float channelMinAbsDist[3] = { 1e10f, 1e10f, 1e10f };
        float channelSignedDist[3] = { 0.0f, 0.0f, 0.0f };

        for (const auto& contour : contours) {
            for (const auto& edge : contour.edges) {
                float dist = SignedDistance(p, edge);
                float absDist = std::abs(dist);

                // Update each channel this edge contributes to
                for (int ch = 0; ch < 3; ++ch) {
                    if (edge.colorMask & (1 << ch)) {
                        if (absDist < channelMinAbsDist[ch]) {
                            channelMinAbsDist[ch] = absDist;
                            channelSignedDist[ch] = dist;
                        }
                    }
                }
            }
        }

        // Convert signed distance to [0, 1]: 0.5 at edge, >0.5 inside, <0.5 outside
        float range = msdf->pixelRange;
        int index = (y * msdf->width + x) * 3;
        for (int ch = 0; ch < 3; ++ch) {
            float val = std::max(0.0f, std::min(1.0f, 0.5f + channelSignedDist[ch] / range));
            msdf->pixels[index + ch] = static_cast<unsigned char>(val * 255.0f);
        }
    }

    void MSDFGenerator::FillMSDF(MSDFData* msdf, std::vector<Contour>& contours) {
        if (!msdf || contours.empty()) return;

        // Find bounding box of all edges
        float minX = 1e10f, minY = 1e10f, maxX = -1e10f, maxY = -1e10f;

        for (const auto& contour : contours) {
            for (const auto& edge : contour.edges) {
                auto updateBounds = [&](const Point& pt) {
                    minX = std::min(minX, pt.x); maxX = std::max(maxX, pt.x);
                    minY = std::min(minY, pt.y); maxY = std::max(maxY, pt.y);
                };
                updateBounds(edge.p0);
                updateBounds(edge.p1);
                if (edge.isQuadratic) updateBounds(edge.p2);
            }
        }

        // Center and scale to fit texture with padding for distance field
        float centerX = (minX + maxX) * 0.5f;
        float centerY = (minY + maxY) * 0.5f;
        float rangeX = maxX - minX;
        float rangeY = maxY - minY;
        float scaleX = static_cast<float>(msdf->width - 1) / std::max(1.0f, rangeX);
        float scaleY = static_cast<float>(msdf->height - 1) / std::max(1.0f, rangeY);
        // Use 85% of space, leaving ~7.5% padding on each side for distance field
        float scale = std::min(scaleX, scaleY) * 0.85f;

        Point center(centerX, centerY);
        Point halfSize(msdf->width * 0.5f, msdf->height * 0.5f);

        // Transform all edges to texture space
        for (auto& contour : contours) {
            for (auto& edge : contour.edges) {
                edge.p0 = (edge.p0 - center) * scale + halfSize;
                edge.p1 = (edge.p1 - center) * scale + halfSize;
                if (edge.isQuadratic) {
                    edge.p2 = (edge.p2 - center) * scale + halfSize;
                }
            }
        }

        // Assign edge colors per contour
        ColorContourEdges(contours);

        // Generate MSDF for each pixel
        for (int y = 0; y < msdf->height; ++y) {
            for (int x = 0; x < msdf->width; ++x) {
                CalculateMSDF(msdf, contours, x, y);
            }
        }
    }

} // namespace FluentUI
