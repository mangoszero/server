#include <vector>
#include <array>
#include "terrain/Accelerators.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace world::terrain
{
    namespace
    {
        constexpr float INF = std::numeric_limits<float>::max();

        inline float AxisOf(const Vec3& v, int a) { return (&v.x)[a]; }

        inline int WidestAxis(const Aabb& b)
        {
            const float dx = b.hi.x - b.lo.x, dy = b.hi.y - b.lo.y, dz = b.hi.z - b.lo.z;
            return (dx > dy) ? ((dx > dz) ? 0 : 2) : ((dy > dz) ? 1 : 2);
        }

        inline float Surface(const Aabb& b)
        {
            if (!b.valid())
            {
                return 0.f;
            }
            const float dx = std::max(0.f, b.hi.x - b.lo.x);
            const float dy = std::max(0.f, b.hi.y - b.lo.y);
            const float dz = std::max(0.f, b.hi.z - b.lo.z);
            return 2.f * (dx * dy + dy * dz + dz * dx);
        }
    }

    static_assert(std::is_trivially_copyable<Bvh::Node>::value,
                  "Bvh::Node must stay trivially copyable (it is written raw to the tile)");

    void Bvh::Build(TriSoup& soup, std::vector<uint16_t>* parallel, int leafSize)
    {
        m_nodes.clear();
        m_maxDepth = 0;
        if (soup.tris.empty())
        {
            return;
        }

        std::vector<uint32_t> order(soup.tris.size());
        for (uint32_t i = 0; i < order.size(); ++i)
        {
            order[i] = i;
        }
        m_nodes.reserve(2 * soup.tris.size() / std::max(1, leafSize) + 8);
        BuildNode(soup, order, 0, uint32_t(order.size()), leafSize, 0);

        std::vector<std::array<uint32_t, 3>> tris(order.size());
        for (size_t i = 0; i < order.size(); ++i)
        {
            tris[i] = soup.tris[order[i]];
        }
        soup.tris.swap(tris);

        if (parallel)
        {
            std::vector<uint16_t> p(order.size());
            for (size_t i = 0; i < order.size(); ++i)
            {
                p[i] = (*parallel)[order[i]];
            }
            parallel->swap(p);
        }
    }

    int Bvh::BuildNode(const TriSoup& soup, std::vector<uint32_t>& order, uint32_t first,
                       uint32_t count, int leafSize, int depth)
    {
        m_maxDepth = std::max(m_maxDepth, depth);

        const int self = int(m_nodes.size());
        m_nodes.push_back(Node{});

        Aabb box, centroids;
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t t = order[first + i];
            box.expand(soup.TriBounds(t));
            centroids.expand(soup.Centroid(t));
        }
        m_nodes[self].box = box;

        if (count <= uint32_t(leafSize) || depth > MAX_DEPTH)
        {
            m_nodes[self].left = -1;
            m_nodes[self].first = first;
            m_nodes[self].count = count;
            return self;
        }

        const int axis = WidestAxis(centroids);
        const float lo = AxisOf(centroids.lo, axis), hi = AxisOf(centroids.hi, axis);
        uint32_t mid = first + count / 2;

        if (hi - lo > 1e-6f)
        {
            constexpr int BINS = 16;
            struct Bin
            {
                Aabb box;
                uint32_t n = 0;
            };
            Bin bins[BINS];
            const float scale = BINS / (hi - lo);

            for (uint32_t i = 0; i < count; ++i)
            {
                const uint32_t t = order[first + i];
                int b = int((AxisOf(soup.Centroid(t), axis) - lo) * scale);
                b = std::min(BINS - 1, std::max(0, b));
                bins[b].n++;
                bins[b].box.expand(soup.TriBounds(t));
            }

            float leftArea[BINS], rightArea[BINS];
            uint32_t leftCnt[BINS], rightCnt[BINS];
            Aabb acc;
            uint32_t n = 0;
            for (int b = 0; b < BINS; ++b)
            {
                acc.expand(bins[b].box);
                n += bins[b].n;
                leftArea[b] = Surface(acc);
                leftCnt[b] = n;
            }
            acc = Aabb{};
            n = 0;
            for (int b = BINS - 1; b >= 0; --b)
            {
                acc.expand(bins[b].box);
                n += bins[b].n;
                rightArea[b] = Surface(acc);
                rightCnt[b] = n;
            }

            float bestCost = INF;
            int bestBin = -1;
            for (int b = 0; b < BINS - 1; ++b)
            {
                if (!leftCnt[b] || !rightCnt[b + 1])
                {
                    continue;
                }
                const float cost = leftArea[b] * leftCnt[b] + rightArea[b + 1] * rightCnt[b + 1];
                if (cost < bestCost)
                {
                    bestCost = cost;
                    bestBin = b;
                }
            }

            if (bestBin >= 0)
            {
                uint32_t* beg = order.data() + first;
                uint32_t* end = beg + count;
                uint32_t* split = std::partition(beg, end, [&](uint32_t t)
                {
                    int b = int((AxisOf(soup.Centroid(t), axis) - lo) * scale);
                    b = std::min(BINS - 1, std::max(0, b));
                    return b <= bestBin;
                });
                mid = first + uint32_t(split - beg);
            }
        }

        if (mid == first || mid == first + count)
        {
            mid = first + count / 2;
        }

        const int l = BuildNode(soup, order, first, mid - first, leafSize, depth + 1);
        const int r = BuildNode(soup, order, mid, first + count - mid, leafSize, depth + 1);
        m_nodes[self].left = l;
        m_nodes[self].right = r;
        m_nodes[self].count = 0;
        return self;
    }

    std::optional<float> Bvh::Raycast(const TriSoup& soup, const Vec3& o, const Vec3& d,
                                      float tMax, uint32_t* hitTri) const
    {
        if (m_nodes.empty())
        {
            return std::nullopt;
        }

        auto inv = [](float v) { return std::fabs(v) > 1e-9f ? 1.f / v : 1e30f; };
        const Vec3 invDir{inv(d.x), inv(d.y), inv(d.z)};

        float best = tMax;
        uint32_t bestTri = 0;
        int stack[MAX_DEPTH + 16];
        int sp = 0;
        stack[sp++] = 0;

        while (sp)
        {
            const Node& n = m_nodes[stack[--sp]];
            if (!n.box.intersectsRay(o, invDir, best))
            {
                continue;
            }
            if (n.left < 0)
            {
                for (uint32_t i = n.first; i < n.first + n.count; ++i)
                {
                    if (auto t = rayTri(o, d, soup.At(i)))
                    {
                        if (*t >= 0.f && *t < best)
                        {
                            best = *t;
                            bestTri = i;
                        }
                    }
                }
                continue;
            }
            // Unconditional: the stack is sized from MAX_DEPTH so it cannot overflow. A
            // bounds check here would silently drop both children on a full stack, losing
            // an entire subtree and any hit inside it with no diagnostic.
            stack[sp++] = n.left;
            stack[sp++] = n.right;
        }

        if (best >= tMax)
        {
            return std::nullopt;
        }
        if (hitTri)
        {
            *hitTri = bestTri;
        }
        return best;
    }

    void Bvh::RaycastAll(const TriSoup& soup, const Vec3& o, const Vec3& d, float tMax,
                         std::vector<Crossing>& out) const
    {
        if (m_nodes.empty())
        {
            return;
        }

        auto inv = [](float v) { return std::fabs(v) > 1e-9f ? 1.f / v : 1e30f; };
        const Vec3 invDir{inv(d.x), inv(d.y), inv(d.z)};

        int stack[MAX_DEPTH + 16];
        int sp = 0;
        stack[sp++] = 0;

        while (sp)
        {
            const Node& n = m_nodes[stack[--sp]];
            if (!n.box.intersectsRay(o, invDir, tMax))
            {
                continue;
            }
            if (n.left < 0)
            {
                for (uint32_t i = n.first; i < n.first + n.count; ++i)
                {
                    if (auto t = rayTri(o, d, soup.At(i)))
                    {
                        if (*t >= 0.f && *t < tMax)
                        {
                            out.push_back(Crossing{*t, i});
                        }
                    }
                }
                continue;
            }
            stack[sp++] = n.left;
            stack[sp++] = n.right;
        }
    }
}
