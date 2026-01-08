#ifndef AI_RUBIKS_H
#define AI_RUBIKS_H

#include <array>
#include <cstdint>
#include <fstream>
#include <functional>
#include <memory>
#include <queue>
#include <unordered_map>

class RubiksCube;

// Moves use standard notation: clockwise, prime, and half turns
enum Move : int {
    U, U_PRIME, U2,  // Up
    D, D_PRIME, D2,  // Down
    L, L_PRIME, L2,  // Left
    R, R_PRIME, R2,  // Right
    F, F_PRIME, F2,  // Front
    B, B_PRIME, B2,  // Back
    MOVE_COUNT
};

// Corner positions
enum Corner { URF=0, UBR=1, DLF=2, DFR=3, ULB=4, UFL=5, DRB=6, DBL=7 };
// Edge positions
enum Edge { eUF=0, eUR=1, eUB=2, eUL=3, eDF=4, eDR=5, eDB=6, eDL=7, eFR=8, eBR=9, eBL=10, eFL=11 };

// Compact position/orientation state (Kociemba/Thistlethwaite style)
struct CompactCube {
    Corner cPos[8] = {URF, UBR, DLF, DFR, ULB, UFL, DRB, DBL};
    int8_t cOri[8] = {0, 0, 0, 0, 0, 0, 0, 0};  // 0, 1, or 2
    Edge ePos[12] = {eUF, eUR, eUB, eUL, eDF, eDR, eDB, eDL, eFR, eBR, eBL, eFL};
    int8_t eOri[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};  // 0 or 1
    std::string path;
    
    CompactCube() = default;
    CompactCube(const CompactCube& other);
    CompactCube& operator=(const CompactCube& other);
    bool operator==(const CompactCube& other) const;
    
    void rotU(int amount = 1);
    void rotD(int amount = 1);
    void rotL(int amount = 1);
    void rotR(int amount = 1);
    void rotF(int amount = 1);
    void rotB(int amount = 1);
    void applyMove(char face, int amount);
};

// Sticker colors (54 entries = 6 faces × 9)
using CubeState = std::array<int, 54>;

class Solver {
public:
    Solver(RubiksCube& c, const std::string& cacheDir = ".");
    ~Solver() = default;

    std::vector<Move> solve();
    std::vector<Move> solveFromState(const CubeState& stateSnapshot);
    
    static CubeState applyMove(const CubeState& state, Move move);
    
    void executeMoves(const std::vector<Move>& moves, float duration = 0.3f);
    
    static bool isSolved(const CubeState& state);
    
    static std::string moveToString(Move m);
    
    static std::vector<Move> parseMoveString(const std::string& moveStr);
    
    // Exposed for debugging hooks
    CompactCube stateToCompact(const CubeState& state);

private:
    // Phase ID helpers
    int64_t idPhase1(const CompactCube& c);
    int64_t idPhase2(const CompactCube& c);
    int64_t idPhase3(const CompactCube& c);
    int64_t idPhase4(const CompactCube& c);
    int64_t getPhaseId(const CompactCube& c, int phase);
    
    // Phase-table lookup
    std::string lookupSolution(const CompactCube& c, int phase);
    
    std::vector<Move> solveWithTables(CompactCube startCube);
    
    bool loadDatabaseFiles();
    void loadDatabaseFile(const std::string& filename, int phase);
    
    void generatePhaseTable(int phase);
    void saveDatabaseFile(const std::string& filename, int phase);
    
    // Move gating per phase
    void nextPhase();
    void resetAllowedMoves();
    bool allowedMoves[18];
    int currentPhase = 1;
    
    // Phase goal IDs (from solved cube)
    int64_t phaseGoal[5];
    
    // Sticker-level move helpers
    static void rotateFaceCW(CubeState& state, int face);
    static void rotateFaceCCW(CubeState& state, int face);
    static void rotateFace180(CubeState& state, int face);
    
    RubiksCube& cube;
    std::string cacheDirectory;
    
    // phase ID -> solution moves
    std::unordered_map<int64_t, std::string> phaseTable[4];
    bool tablesLoaded = false;
    
    // Corner and edge name tables for phase 3/4 encoding
    inline static const std::string cornerNames[8] = {"URF", "UBR", "DLF", "DFR", "ULB", "UFL", "DRB", "DBL"};
    inline static const std::string edgeNames[12] = {"UF", "UR", "UB", "UL", "DF", "DR", "DB", "DL", "FR", "BR", "BL", "FL"};
    inline static const std::string faces = "FRUBLD";
    
    // Database file names
    static constexpr const char* PHASE1_DB = "phase1";
    static constexpr const char* PHASE2_DB = "phase2";
    static constexpr const char* PHASE3_DB = "phase3";
    static constexpr const char* PHASE4_DB = "phase4";
};

#endif // AI_RUBIKS_H
