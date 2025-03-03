/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_H_BIH
#define MANGOS_H_BIH

#include <G3D/Vector3.h>
#include <G3D/Ray.h>
#include <G3D/AABox.h>

#include <Platform/Define.h>

#include <stdexcept>
#include <vector>
#include <algorithm>
#include <limits>
#include <cmath>

#define MAX_STACK_SIZE 64

#ifdef _MSC_VER
#define isnan(x) _isnan(x)
#else
#define isnan(x) std::isnan(x)
#endif

using G3D::Vector3;
using G3D::AABox;
using G3D::Ray;

/**
 * @brief Converts a float to its raw integer bits representation.
 *
 * @param f The float value to convert.
 * @return uint32 The raw integer bits of the float.
 */
static inline uint32 floatToRawIntBits(float f)
{
    union
    {
        uint32 ival;
        float fval;
    } temp;
    temp.fval = f;
    return temp.ival;
}

/**
 * @brief Converts raw integer bits to a float.
 *
 * @param i The raw integer bits.
 * @return float The float value.
 */
static inline float intBitsToFloat(uint32 i)
{
    union
    {
        uint32 ival;
        float fval;
    } temp;
    temp.ival = i;
    return temp.fval;
}

/**
 * @brief Structure representing an axis-aligned bounding box.
 */
struct AABound
{
    Vector3 lo, hi; /**< Lower and upper bounds of the box. */
};

/**
 * @brief Bounding Interval Hierarchy Class.
 *
 * Building and Ray-Intersection functions based on BIH from
 * Sunflow, a Java Raytracer, released under MIT/X11 License
 * http://sunflow.sourceforge.net/
 * Copyright (c) 2003-2007 Christopher Kulla
 */
class BIH
{
private:
    /**
     * @brief Initializes an empty BIH.
     */
    void init_empty()
    {
        tree.clear();
        objects.clear();
        // Create space for the first node (dummy leaf)
        tree.push_back((uint32)3 << 30);
        tree.insert(tree.end(), 2, 0);
    }

public:
    /**
     * @brief Default constructor for BIH.
     */
    BIH() { init_empty(); }

    /**
     * @brief Builds the BIH tree.
     *
     * @tparam BoundsFunc Function type for getting bounds.
     * @tparam PrimArray Array type for primitives.
     * @param primitives Array of primitives.
     * @param getBounds Function to get bounds of a primitive.
     * @param leafSize Maximum number of primitives in a leaf node.
     * @param printStats Whether to print build statistics.
     */
    template<class BoundsFunc, class PrimArray>
    void build(const PrimArray& primitives, BoundsFunc& getBounds, uint32 leafSize = 3, bool printStats = false)
    {
        if (primitives.size() == 0)
        {
            init_empty();
            return;
        }

        buildData dat{};
        dat.maxPrims = leafSize;
        dat.numPrims = primitives.size();
        dat.indices = new uint32[dat.numPrims];
        dat.primBound = new AABox[dat.numPrims];
        getBounds(primitives[0], bounds);

        for (uint32 i = 0; i < dat.numPrims; ++i)
        {
            dat.indices[i] = i;
            getBounds(primitives[i], dat.primBound[i]);
            bounds.merge(dat.primBound[i]);
        }

        std::vector<uint32> tempTree;
        BuildStats stats;
        buildHierarchy(tempTree, dat, stats);

        if (printStats)
        {
            stats.printStats();
        }

        objects.resize(dat.numPrims);
        for (uint32 i = 0; i < dat.numPrims; ++i)
        {
            objects[i] = dat.indices[i];
        }

        tree = tempTree;
        delete[] dat.primBound;
        delete[] dat.indices;
    }

    /**
     * @brief Returns the number of primitives in the BIH.
     *
     * @return uint32 Number of primitives.
     */
    uint32 primCount() { return objects.size(); }

    /**
     * @brief Intersects a ray with the BIH.
     *
     * @tparam RayCallback Callback type for intersection.
     * @param r The ray to intersect.
     * @param intersectCallback The callback to handle intersections.
     * @param maxDist Maximum distance for intersection.
     * @param stopAtFirst Whether to stop at the first intersection.
     */
    template<typename RayCallback>
    void intersectRay(const Ray& r, RayCallback& intersectCallback, float& maxDist, bool stopAtFirst = false) const
    {
        float intervalMin = -1.f;
        float intervalMax = -1.f;
        Vector3 const& org = r.origin();
        Vector3 const& dir = r.direction();
        Vector3 const& invDir = r.invDirection();

        for (int i = 0; i < 3; ++i)
        {
            if (G3D::fuzzyNe(dir[i], 0.0f))
            {
                float t1 = (bounds.low()[i] - org[i]) * invDir[i];
                float t2 = (bounds.high()[i] - org[i]) * invDir[i];
                if (t1 > t2)
                {
                    std::swap(t1, t2);
                }
                if (t1 > intervalMin)
                {
                    intervalMin = t1;
                }
                if (t2 < intervalMax || intervalMax < 0.f)
                {
                    intervalMax = t2;
                }
                // intervalMax can only become smaller for other axis,
                //  and intervalMin only larger respectively, so stop early
                if (intervalMax <= 0 || intervalMin >= maxDist)
                {
                    return;
                }
            }
        }

        if (intervalMin > intervalMax)
        {
            return;
        }

        intervalMin = std::max(intervalMin, 0.f);
        intervalMax = std::min(intervalMax, maxDist);

        uint32 offsetFront[3]{};
        uint32 offsetBack[3]{};
        uint32 offsetFront3[3]{};
        uint32 offsetBack3[3]{};
            // compute custom offsets from direction sign bit

        for (int i = 0; i < 3; ++i)
        {
            offsetFront[i] = floatToRawIntBits(dir[i]) >> 31;
            offsetBack[i] = offsetFront[i] ^ 1;
            offsetFront3[i] = offsetFront[i] * 3;
            offsetBack3[i] = offsetBack[i] * 3;

                // avoid always adding 1 during the inner loop
            ++offsetFront[i];
            ++offsetBack[i];
        }

        StackNode stack[MAX_STACK_SIZE];
        int stackPos = 0;
        int node = 0;

        while (true)
        {
            while (true)
            {
                uint32 tn = tree[node];
                uint32 axis = (tn & (3 << 30)) >> 30;
                bool BVH2 = tn & (1 << 29);
                int offset = tn & ~(7 << 29);

                if (!BVH2)
                {
                    if (axis < 3)
                    {
                            // "normal" interior node
                        float tf = (intBitsToFloat(tree[node + offsetFront[axis]]) - org[axis]) * invDir[axis];
                        float tb = (intBitsToFloat(tree[node + offsetBack[axis]]) - org[axis]) * invDir[axis];
                        // ray passes between clip zones
                        if (tf < intervalMin && tb > intervalMax)
                        {
                            break;
                        }

                        int back = offset + offsetBack3[axis];
                        node = back;
                        // ray passes through far node only
                        if (tf < intervalMin)
                        {
                            intervalMin = (tb >= intervalMin) ? tb : intervalMin;
                            continue;
                        }

                        node = offset + offsetFront3[axis]; // front
                        // ray passes through near node only
                        if (tb > intervalMax)
                        {
                            intervalMax = (tf <= intervalMax) ? tf : intervalMax;
                            continue;
                        }
                        // ray passes through both nodes
                        // push back node
                        stack[stackPos].node = back;
                        stack[stackPos].tnear = (tb >= intervalMin) ? tb : intervalMin;
                        stack[stackPos].tfar = intervalMax;
                        ++stackPos;
                        // update ray interval for front node
                        intervalMax = (tf <= intervalMax) ? tf : intervalMax;
                        continue;
                    }
                    else
                    {
                        // leaf - test some objects
                        int n = tree[node + 1];
                        while (n > 0)
                        {
                            bool hit = intersectCallback(r, objects[offset], maxDist, stopAtFirst);
                            if (stopAtFirst && hit)
                            {
                                return;
                            }
                            --n;
                            ++offset;
                        }
                        break;
                    }
                }
                else
                {
                    if (axis > 2)
                    {
                        return;  // should not happen
                    }

                    float tf = (intBitsToFloat(tree[node + offsetFront[axis]]) - org[axis]) * invDir[axis];
                    float tb = (intBitsToFloat(tree[node + offsetBack[axis]]) - org[axis]) * invDir[axis];
                    node = offset;
                    intervalMin = (tf >= intervalMin) ? tf : intervalMin;
                    intervalMax = (tb <= intervalMax) ? tb : intervalMax;

                    if (intervalMin > intervalMax)
                    {
                        break;
                    }
                    continue;
                }
            } // traversal loop

            do
            {
                // stack is empty?
                if (stackPos == 0)
                {
                    return;
                }
                // move back up the stack
                --stackPos;
                intervalMin = stack[stackPos].tnear;
                if (maxDist < intervalMin)
                {
                    continue;
                }
                node = stack[stackPos].node;
                intervalMax = stack[stackPos].tfar;
                break;
            }
            while (true);
        }
    }

    /**
     * @brief Intersects a point with the BIH.
     *
     * @tparam IsectCallback Callback type for intersection.
     * @param p The point to intersect.
     * @param intersectCallback The callback to handle intersections.
     */
    template<typename IsectCallback>
    void intersectPoint(const Vector3& p, IsectCallback& intersectCallback) const
    {
        if (!bounds.contains(p))
        {
            return;
        }

        StackNode stack[MAX_STACK_SIZE];
        int stackPos = 0;
        int node = 0;

        while (true)
        {
            while (true)
            {
                uint32 tn = tree[node];
                uint32 axis = (tn & (3 << 30)) >> 30;
                bool BVH2 = tn & (1 << 29);
                int offset = tn & ~(7 << 29);

                if (!BVH2)
                {
                    if (axis < 3)
                    {
                        // "normal" interior node
                        float tl = intBitsToFloat(tree[node + 1]);
                        float tr = intBitsToFloat(tree[node + 2]);
                        // point is between clip zones
                        if (tl < p[axis] && tr > p[axis])
                        {
                            break;
                        }

                        int right = offset + 3;
                        node = right;
                        // point is in right node only
                        if (tl < p[axis])
                        {
                            continue;
                        }

                        node = offset; // left
                        // point is in left node only
                        if (tr > p[axis])
                        {
                            continue;
                        }
                        // point is in both nodes
                        // push back right node
                        stack[stackPos].node = right;
                        ++stackPos;
                        continue;
                    }
                    else
                    {
                        // leaf - test some objects
                        int n = tree[node + 1];
                        while (n > 0)
                        {
                            intersectCallback(p, objects[offset]);
                            --n;
                            ++offset;
                        }
                        break;
                    }
                }
                else // BVH2 node (empty space cut off left and right)
                {
                    if (axis > 2)
                    {
                        return;  // should not happen
                    }

                    float tl = intBitsToFloat(tree[node + 1]);
                    float tr = intBitsToFloat(tree[node + 2]);
                    node = offset;

                    if (tl > p[axis] || tr < p[axis])
                    {
                        break;
                    }
                    continue;
                }
            } // traversal loop

            // stack is empty?
            if (stackPos == 0)
            {
                return;
            }
            // move back up the stack
            --stackPos;
            node = stack[stackPos].node;
        }
    }

    /**
     * @brief Writes the BIH tree to a file.
     *
     * @param wf File pointer to write to.
     * @return bool True if the write was successful, false otherwise.
     */
    bool WriteToFile(FILE* wf) const;

    /**
     * @brief Reads the BIH tree from a file.
     *
     * @param rf File pointer to read from.
     * @return bool True if the read was successful, false otherwise.
     */
    bool ReadFromFile(FILE* rf);

protected:
    std::vector<uint32> tree; /**< The BIH tree structure. */
    std::vector<uint32> objects; /**< The objects in the BIH. */
    AABox bounds; /**< The bounding box of the BIH. */

    /**
     * @brief Structure for build data.
     */
    struct buildData
    {
        uint32* indices; /**< Indices of the primitives. */
        AABox* primBound; /**< Bounding boxes of the primitives. */
        uint32 numPrims; /**< Number of primitives. */
        int maxPrims; /**< Maximum number of primitives in a leaf node. */
    };

    /**
     * @brief Structure for stack nodes during traversal.
     */
    struct StackNode
    {
        uint32 node; /**< Node index. */
        float tnear; /**< Near distance. */
        float tfar; /**< Far distance. */
    };

    /**
     * @brief Class for build statistics.
     */
    class BuildStats
    {
    private:
        int numNodes; /**< Number of nodes. */
        int numLeaves; /**< Number of leaves. */
        int sumObjects; /**< Sum of objects. */
        int minObjects; /**< Minimum number of objects in a leaf. */
        int maxObjects; /**< Maximum number of objects in a leaf. */
        int sumDepth; /**< Sum of depths. */
        int minDepth; /**< Minimum depth. */
        int maxDepth; /**< Maximum depth. */
        int numLeavesN[6]; /**< Number of leaves with N objects. */
        int numBVH2; /**< Number of BVH2 nodes. */

    public:
        /**
         * @brief Default constructor for BuildStats.
         */
        BuildStats() :
            numNodes(0), numLeaves(0), sumObjects(0), minObjects(0x0FFFFFFF),
            maxObjects(0xFFFFFFFF), sumDepth(0), minDepth(0x0FFFFFFF),
            maxDepth(0xFFFFFFFF), numBVH2(0)
        {
            for (int i = 0; i < 6; ++i)
            {
                numLeavesN[i] = 0;
            }
        }

        /**
         * @brief Updates statistics for an inner node.
         */
        void updateInner() { ++numNodes; }

        /**
         * @brief Updates statistics for a BVH2 node.
         */
        void updateBVH2() { ++numBVH2; }

        /**
         * @brief Updates statistics for a leaf node.
         *
         * @param depth Depth of the leaf node.
         * @param n Number of objects in the leaf node.
         */
        void updateLeaf(int depth, int n);

        /**
         * @brief Prints the build statistics.
         */
        void printStats();
    };

    /**
     * @brief Builds the BIH hierarchy.
     *
     * @param tempTree Temporary tree structure.
     * @param dat Build data containing primitives and bounds.
     * @param stats Statistics for the build process.
     */
    void buildHierarchy(std::vector<uint32>& tempTree, buildData& dat, BuildStats& stats);

    /**
     * @brief Creates a leaf node in the BIH tree.
     *
     * @param tempTree Temporary tree structure.
     * @param nodeIndex Index of the current node.
     * @param left Left index of the range.
     * @param right Right index of the range.
     */
    void createNode(std::vector<uint32>& tempTree, int nodeIndex, uint32 left, uint32 right)
    {
        // Write leaf node
        tempTree[nodeIndex + 0] = (3 << 30) | left;
        tempTree[nodeIndex + 1] = right - left + 1;
    }

    /**
     * @brief Subdivides the BIH tree.
     *
     * @param left Left index of the range.
     * @param right Right index of the range.
     * @param tempTree Temporary tree structure.
     * @param dat Build data containing primitives and bounds.
     * @param gridBox Grid bounding box.
     * @param nodeBox Node bounding box.
     * @param nodeIndex Index of the current node.
     * @param depth Current depth of the tree.
     * @param stats Statistics for the build process.
     */
    void subdivide(int left, int right, std::vector<uint32>& tempTree, buildData& dat, AABound& gridBox, AABound& nodeBox, int nodeIndex, int depth, BuildStats& stats);
};

#endif // MANGOS_H_BIH
