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

#include "BIH.h"

 /**
  * @brief Builds the BIH hierarchy.
  *
  * @param tempTree Temporary tree structure.
  * @param dat Build data containing primitives and bounds.
  * @param stats Statistics for the build process.
  */
void BIH::buildHierarchy(std::vector<uint32>& tempTree, buildData& dat, BuildStats& stats)
{
    // Create space for the first node (dummy leaf)
    tempTree.push_back((uint32)3 << 30);
    tempTree.insert(tempTree.end(), 2, 0);

    // Seed bounding box
    AABound gridBox = { bounds.low(), bounds.high() };
    AABound nodeBox = gridBox;

    // Seed subdivide function
    subdivide(0, dat.numPrims - 1, tempTree, dat, gridBox, nodeBox, 0, 1, stats);
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
void BIH::subdivide(int left, int right, std::vector<uint32>& tempTree, buildData& dat, AABound& gridBox, AABound& nodeBox, int nodeIndex, int depth, BuildStats& stats)
{
    // Check if we should create a leaf node
    if ((right - left + 1) <= dat.maxPrims || depth >= MAX_STACK_SIZE)
    {
        // write leaf node
        stats.updateLeaf(depth, right - left + 1);
        createNode(tempTree, nodeIndex, left, right);
        return;
    }
    // calculate extents
    // Initialize variables for partitioning
    int axis = -1, rightOrig;
    float clipL;
    float clipR;
    float prevClip = G3D::fnan();
    float split = G3D::fnan();
    bool wasLeft = true;

    while (true)
    {
        int prevAxis = axis;
        float prevSplit = split;

        // Perform quick consistency checks
        Vector3 d(gridBox.hi - gridBox.lo);
        if (d.x < 0 || d.y < 0 || d.z < 0)
        {
            throw std::logic_error("negative node extents");
        }
        for (int i = 0; i < 3; ++i)
        {
            if (nodeBox.hi[i] < gridBox.lo[i] || nodeBox.lo[i] > gridBox.hi[i])
            {
                throw std::logic_error("invalid node overlap");
            }
        }

        // Find the longest axis
        axis = d.primaryAxis();
        split = 0.5f * (gridBox.lo[axis] + gridBox.hi[axis]);

        // Partition L/R subsets
        clipL = -G3D::inf();
        clipR = G3D::inf();
        rightOrig = right; // Save this for later
        float nodeL = G3D::inf();
        float nodeR = -G3D::inf();

        for (int i = left; i <= right;)
        {
            int obj = dat.indices[i];
            float minb = dat.primBound[obj].low()[axis];
            float maxb = dat.primBound[obj].high()[axis];
            float center = (minb + maxb) * 0.5f;

            if (center <= split)
            {
                // Stay left
                ++i;
                if (clipL < maxb)
                {
                    clipL = maxb;
                }
            }
            else
            {
                // Move to the right
                int t = dat.indices[i];
                dat.indices[i] = dat.indices[right];
                dat.indices[right] = t;
                --right;
                if (clipR > minb)
                {
                    clipR = minb;
                }
            }
            nodeL = std::min(nodeL, minb);
            nodeR = std::max(nodeR, maxb);
        }

        // Check for empty space
        if (nodeL > nodeBox.lo[axis] && nodeR < nodeBox.hi[axis])
        {
            float nodeBoxW = nodeBox.hi[axis] - nodeBox.lo[axis];
            float nodeNewW = nodeR - nodeL;

            // Node box is too big compared to space occupied by primitives
            if (1.3f * nodeNewW < nodeBoxW)
            {
                stats.updateBVH2();
                int nextIndex = tempTree.size();

                // Allocate child
                tempTree.push_back(0);
                tempTree.push_back(0);
                tempTree.push_back(0);

                // Write BVH2 clip node
                stats.updateInner();
                tempTree[nodeIndex + 0] = (axis << 30) | (1 << 29) | nextIndex;
                tempTree[nodeIndex + 1] = floatToRawIntBits(nodeL);
                tempTree[nodeIndex + 2] = floatToRawIntBits(nodeR);

                // Update node box and recurse
                nodeBox.lo[axis] = nodeL;
                nodeBox.hi[axis] = nodeR;
                subdivide(left, rightOrig, tempTree, dat, gridBox, nodeBox, nextIndex, depth + 1, stats);
                return;
            }
        }

        // Ensure we are making progress in the subdivision
        if (right == rightOrig)
        {
            // All left
            if (prevAxis == axis && G3D::fuzzyEq(prevSplit, split))
            {
                // We are stuck here - create a leaf
                stats.updateLeaf(depth, right - left + 1);
                createNode(tempTree, nodeIndex, left, right);
                return;
            }
            if (clipL <= split)
            {
                // Keep looping on left half
                gridBox.hi[axis] = split;
                prevClip = clipL;
                wasLeft = true;
                continue;
            }
            gridBox.hi[axis] = split;
            prevClip = G3D::fnan();
        }
        else if (left > right)
        {
            // All right
            right = rightOrig;
            if (prevAxis == axis && G3D::fuzzyEq(prevSplit, split))
            {
                // We are stuck here - create a leaf
                stats.updateLeaf(depth, rightOrig - left + 1);
                createNode(tempTree, nodeIndex, left, rightOrig);
                return;
            }
            if (clipR >= split)
            {
                // Keep looping on right half
                gridBox.lo[axis] = split;
                prevClip = clipR;
                wasLeft = false;
                continue;
            }
            gridBox.lo[axis] = split;
            prevClip = G3D::fnan();
        }
        else
        {
            // We are actually splitting stuff
            if (prevAxis != -1 && !isnan(prevClip))
            {
                // Second time through - create the previous split since it produced empty space
                int nextIndex = tempTree.size();

                // Allocate child node
                tempTree.push_back(0);
                tempTree.push_back(0);
                tempTree.push_back(0);

                if (wasLeft)
                {
                    // Create a node with a left child
                    // write leaf node
                    stats.updateInner();
                    tempTree[nodeIndex + 0] = (prevAxis << 30) | nextIndex;
                    tempTree[nodeIndex + 1] = floatToRawIntBits(prevClip);
                    tempTree[nodeIndex + 2] = floatToRawIntBits(G3D::inf());
                }
                else
                {
                    // Create a node with a right child
                    stats.updateInner();
                    tempTree[nodeIndex + 0] = (prevAxis << 30) | (nextIndex - 3);
                    tempTree[nodeIndex + 1] = floatToRawIntBits(-G3D::inf());
                    tempTree[nodeIndex + 2] = floatToRawIntBits(prevClip);
                }

                // Count stats for the unused leaf
                ++depth;
                stats.updateLeaf(depth, 0);

                // Now we keep going as we are, with a new nodeIndex
                nodeIndex = nextIndex;
            }
            break;
        }
    }

    // Compute index of child nodes
    int nextIndex = tempTree.size();

    // Allocate left node
    int nl = right - left + 1;
    int nr = rightOrig - (right + 1) + 1;
    if (nl > 0)
    {
        tempTree.push_back(0);
        tempTree.push_back(0);
        tempTree.push_back(0);
    }
    else
    {
        nextIndex -= 3;
    }

    // Allocate right node
    if (nr > 0)
    {
        tempTree.push_back(0);
        tempTree.push_back(0);
        tempTree.push_back(0);
    }

    // Write leaf node
    stats.updateInner();
    tempTree[nodeIndex + 0] = (axis << 30) | nextIndex;
    tempTree[nodeIndex + 1] = floatToRawIntBits(clipL);
    tempTree[nodeIndex + 2] = floatToRawIntBits(clipR);

    // Prepare L/R child boxes
    AABound gridBoxL(gridBox), gridBoxR(gridBox);
    AABound nodeBoxL(nodeBox), nodeBoxR(nodeBox);
    gridBoxL.hi[axis] = gridBoxR.lo[axis] = split;
    nodeBoxL.hi[axis] = clipL;
    nodeBoxR.lo[axis] = clipR;

    // Recurse
    if (nl > 0)
    {
        subdivide(left, right, tempTree, dat, gridBoxL, nodeBoxL, nextIndex, depth + 1, stats);
    }
    else
    {
        stats.updateLeaf(depth + 1, 0);
    }
    if (nr > 0)
    {
        subdivide(right + 1, rightOrig, tempTree, dat, gridBoxR, nodeBoxR, nextIndex + 3, depth + 1, stats);
    }
    else
    {
        stats.updateLeaf(depth + 1, 0);
    }
}

/**
 * @brief Writes the BIH tree to a file.
 *
 * @param wf File pointer to write to.
 * @return true if the write was successful, false otherwise.
 */
bool BIH::WriteToFile(FILE* wf) const
{
    uint32 treeSize = tree.size();
    uint32 check = 0;
    uint32 count = objects.size();

    // Write bounds, tree size, tree data, and object count to file
    check += fwrite(&bounds.low(), sizeof(float), 3, wf);
    check += fwrite(&bounds.high(), sizeof(float), 3, wf);
    check += fwrite(&treeSize, sizeof(uint32), 1, wf);
    check += fwrite(&tree[0], sizeof(uint32), treeSize, wf);
    check += fwrite(&count, sizeof(uint32), 1, wf);
    check += fwrite(&objects[0], sizeof(uint32), count, wf);

    // Return true if all writes were successful
    return check == (3 + 3 + 2 + treeSize + count);
}

/**
 * @brief Reads the BIH tree from a file.
 *
 * @param rf File pointer to read from.
 * @return true if the read was successful, false otherwise.
 */
bool BIH::ReadFromFile(FILE* rf)
{
    uint32 treeSize;
    Vector3 lo, hi;
    uint32 check = 0, count = 0;

    // Read bounds, tree size, tree data, and object count from file
    check += fread(&lo, sizeof(float), 3, rf);
    check += fread(&hi, sizeof(float), 3, rf);
    bounds = AABox(lo, hi);
    check += fread(&treeSize, sizeof(uint32), 1, rf);
    tree.resize(treeSize);
    check += fread(&tree[0], sizeof(uint32), treeSize, rf);
    check += fread(&count, sizeof(uint32), 1, rf);
    objects.resize(count);
    check += fread(&objects[0], sizeof(uint32), count, rf);

    // Return true if all reads were successful
    return check == (3 + 3 + 2 + treeSize + count);
}

/**
 * @brief Updates the build statistics for a leaf node.
 *
 * @param depth Depth of the leaf node.
 * @param n Number of objects in the leaf node.
 */
void BIH::BuildStats::updateLeaf(int depth, int n)
{
    ++numLeaves;
    minDepth = std::min(depth, minDepth);
    maxDepth = std::max(depth, maxDepth);
    sumDepth += depth;
    minObjects = std::min(n, minObjects);
    maxObjects = std::max(n, maxObjects);
    sumObjects += n;
    int nl = std::min(n, 5);
    ++numLeavesN[nl];
}

/**
 * @brief Prints the build statistics.
 */
void BIH::BuildStats::printStats()
{
    printf("Tree stats:\n");
    printf("  * Nodes:          %d\n", numNodes);
    printf("  * Leaves:         %d\n", numLeaves);
    printf("  * Objects: min    %d\n", minObjects);
    printf("             avg    %.2f\n", (float)sumObjects / numLeaves);
    printf("           avg(n>0) %.2f\n", (float)sumObjects / (numLeaves - numLeavesN[0]));
    printf("             max    %d\n", maxObjects);
    printf("  * Depth:   min    %d\n", minDepth);
    printf("             avg    %.2f\n", (float)sumDepth / numLeaves);
    printf("             max    %d\n", maxDepth);
    printf("  * Leaves w/: N=0  %3d%%\n", 100 * numLeavesN[0] / numLeaves);
    printf("               N=1  %3d%%\n", 100 * numLeavesN[1] / numLeaves);
    printf("               N=2  %3d%%\n", 100 * numLeavesN[2] / numLeaves);
    printf("               N=3  %3d%%\n", 100 * numLeavesN[3] / numLeaves);
    printf("               N=4  %3d%%\n", 100 * numLeavesN[4] / numLeaves);
    printf("               N>4  %3d%%\n", 100 * numLeavesN[5] / numLeaves);
    printf("  * BVH2 nodes:     %d (%3d%%)\n", numBVH2, 100 * numBVH2 / (numNodes + numLeaves - 2 * numBVH2));
}
