#include "CubeConversion.h"

#include <algorithm>
#include <array>

namespace CubeConversion {
namespace {
constexpr std::array<std::pair<int, int>, 12> EDGES = {{
    {19, 37}, {23, 1}, {25, 46}, {21, 10},
    {34, 43}, {32, 7}, {28, 52}, {30, 16},
    {41, 3},  {48, 5}, {50, 12}, {39, 14}
}};

constexpr std::array<std::array<int, 3>, 8> CORNERS = {{
    std::array<int, 3>{20, 0, 38}, std::array<int, 3>{26, 45, 2},
    std::array<int, 3>{33, 17, 42}, std::array<int, 3>{35, 44, 6},
    std::array<int, 3>{24, 9, 47}, std::array<int, 3>{18, 36, 11},
    std::array<int, 3>{29, 8, 51}, std::array<int, 3>{27, 53, 15}
}};

constexpr int solvedCornerColors[8][3] = {
    {2, 0, 4}, {2, 5, 0}, {3, 1, 4}, {3, 4, 0},
    {2, 1, 5}, {2, 4, 1}, {3, 0, 5}, {3, 5, 1}
};

constexpr int solvedEdgeColors[12][2] = {
    {2, 4}, {2, 0}, {2, 5}, {2, 1},
    {3, 4}, {3, 0}, {3, 5}, {3, 1},
    {4, 0}, {5, 0}, {5, 1}, {4, 1}
};
} // namespace

CubeState compactToState(const CompactCube& cube) {
    CubeState s{};
    for (auto& v : s) v = -1;

    s[4] = 0;
    s[13] = 1;
    s[22] = 2;
    s[31] = 3;
    s[40] = 4;
    s[49] = 5;

    for (int pos = 0; pos < 8; ++pos) {
        const auto piece = static_cast<int>(cube.cPos[pos]);
        const int ori = cube.cOri[pos];
        for (int j = 0; j < 3; ++j) {
            int color = solvedCornerColors[piece][(j - ori + 3) % 3];
            s[CORNERS[pos][j]] = color;
        }
    }

    for (int pos = 0; pos < 12; ++pos) {
        const auto piece = static_cast<int>(cube.ePos[pos]);
        const int ori = cube.eOri[pos];
        int c0 = solvedEdgeColors[piece][(0 - ori + 2) % 2];
        int c1 = solvedEdgeColors[piece][(1 - ori + 2) % 2];
        s[EDGES[pos].first] = c0;
        s[EDGES[pos].second] = c1;
    }

    return s;
}

CompactCube stateToCompact(const CubeState& state) {
    CompactCube cube;

    for (int pos = 0; pos < 8; ++pos) {
        std::array<int, 3> colors;
        for (int j = 0; j < 3; ++j) colors[j] = state[CORNERS[pos][j]];
        auto sortedColors = colors;
        std::sort(sortedColors.begin(), sortedColors.end());

        for (int piece = 0; piece < 8; ++piece) {
            std::array<int, 3> pieceColors = {
                solvedCornerColors[piece][0], solvedCornerColors[piece][1], solvedCornerColors[piece][2]};
            auto sortedPiece = pieceColors;
            std::sort(sortedPiece.begin(), sortedPiece.end());

            if (sortedColors == sortedPiece) {
                cube.cPos[pos] = static_cast<Corner>(piece);
                int primary = solvedCornerColors[piece][0];
                if (colors[0] == primary) cube.cOri[pos] = 0;
                else if (colors[1] == primary) cube.cOri[pos] = 1;
                else cube.cOri[pos] = 2;
                break;
            }
        }
    }

    for (int pos = 0; pos < 12; ++pos) {
        std::array<int, 2> colors = {state[EDGES[pos].first], state[EDGES[pos].second]};
        auto sortedColors = colors;
        std::sort(sortedColors.begin(), sortedColors.end());

        for (int piece = 0; piece < 12; ++piece) {
            std::array<int, 2> pieceColors = {solvedEdgeColors[piece][0], solvedEdgeColors[piece][1]};
            auto sortedPiece = pieceColors;
            std::sort(sortedPiece.begin(), sortedPiece.end());

            if (sortedColors == sortedPiece) {
                cube.ePos[pos] = static_cast<Edge>(piece);
                cube.eOri[pos] = (colors[0] == pieceColors[0]) ? 0 : 1;
                break;
            }
        }
    }

    return cube;
}

} // namespace CubeConversion
