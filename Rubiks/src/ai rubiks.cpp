#include "ai rubiks.h"
#include "CubeConversion.h"
#include "RubiksCube.h"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace {
    // CubeState face order: Right, Left, Up, Down, Front, Back.
    constexpr int RIGHT = 0, LEFT = 1, UP = 2, DOWN = 3, FRONT = 4, BACK = 5;
    
    // Sticker indices for each edge (U/D/F/B sticker first, partner face second)
    constexpr std::array<std::pair<int, int>, 12> EDGES = {{
        {19, 37},  // UF
        {23, 1},   // UR
        {25, 46},  // UB
        {21, 10},  // UL
        {34, 43},  // DF
        {32, 7},   // DR
        {28, 52},  // DB
        {30, 16},  // DL
        {41, 3},   // FR
        {48, 5},   // BR
        {50, 12},  // BL
        {39, 14},  // FL
    }};
    
    // Sticker indices for each corner (U/D, then R/L, then F/B orientation)
    constexpr std::array<std::array<int, 3>, 8> CORNERS = {{
        {20, 0, 38},   // URF at (1,1,1)
        {26, 45, 2},   // UBR at (1,1,-1)
        {33, 17, 42},  // DLF at (-1,-1,1)
        {35, 44, 6},   // DFR at (1,-1,1)
        {24, 9, 47},   // ULB at (-1,1,-1)
        {18, 36, 11},  // UFL at (-1,1,1)
        {29, 8, 51},   // DRB at (1,-1,-1)
        {27, 53, 15},  // DBL at (-1,-1,-1)
    }};
    
    char moves[6] = {'F', 'R', 'U', 'B', 'L', 'D'};

    // Solver database encodes moves as: 1=one CW turn, 2=two turns (180°), 3=three CW turns (=CCW).
    // This matches the reference solver exactly.
}

CompactCube::CompactCube(const CompactCube& other) {
    for (int i = 0; i < 8; i++) {
        cPos[i] = other.cPos[i];
        cOri[i] = other.cOri[i];
    }
    for (int i = 0; i < 12; i++) {
        ePos[i] = other.ePos[i];
        eOri[i] = other.eOri[i];
    }
    path = other.path;
}

CompactCube& CompactCube::operator=(const CompactCube& other) {
    for (int i = 0; i < 8; i++) {
        cPos[i] = other.cPos[i];
        cOri[i] = other.cOri[i];
    }
    for (int i = 0; i < 12; i++) {
        ePos[i] = other.ePos[i];
        eOri[i] = other.eOri[i];
    }
    path = other.path;
    return *this;
}

bool CompactCube::operator==(const CompactCube& other) const {
    for (int i = 0; i < 8; i++) {
        if (cPos[i] != other.cPos[i] || cOri[i] != other.cOri[i])
            return false;
    }
    for (int i = 0; i < 12; i++) {
        if (ePos[i] != other.ePos[i] || eOri[i] != other.eOri[i])
            return false;
    }
    return true;
}

void CompactCube::rotU(int amount) {
    if (amount < 0) amount = 3;
    for (int a = 0; a < amount; a++) {
        Corner tmp = cPos[URF];
        cPos[URF] = cPos[UBR];
        cPos[UBR] = cPos[ULB];
        cPos[ULB] = cPos[UFL];
        cPos[UFL] = tmp;
        
        int8_t tOri = cOri[URF];
        cOri[URF] = cOri[UBR];
        cOri[UBR] = cOri[ULB];
        cOri[ULB] = cOri[UFL];
        cOri[UFL] = tOri;
        
        tOri = eOri[eUB];
        eOri[eUB] = eOri[eUL];
        eOri[eUL] = eOri[eUF];
        eOri[eUF] = eOri[eUR];
        eOri[eUR] = tOri;
        
        Edge tmp2 = ePos[eUB];
        ePos[eUB] = ePos[eUL];
        ePos[eUL] = ePos[eUF];
        ePos[eUF] = ePos[eUR];
        ePos[eUR] = tmp2;
    }
}

void CompactCube::rotD(int amount) {
    if (amount < 0) amount = 3;
    for (int a = 0; a < amount; a++) {
        Corner tmp = cPos[DFR];
        cPos[DFR] = cPos[DLF];
        cPos[DLF] = cPos[DBL];
        cPos[DBL] = cPos[DRB];
        cPos[DRB] = tmp;
        
        int8_t tOri = cOri[DFR];
        cOri[DFR] = cOri[DLF];
        cOri[DLF] = cOri[DBL];
        cOri[DBL] = cOri[DRB];
        cOri[DRB] = tOri;
        
        tOri = eOri[eDF];
        eOri[eDF] = eOri[eDL];
        eOri[eDL] = eOri[eDB];
        eOri[eDB] = eOri[eDR];
        eOri[eDR] = tOri;
        
        Edge tmp2 = ePos[eDF];
        ePos[eDF] = ePos[eDL];
        ePos[eDL] = ePos[eDB];
        ePos[eDB] = ePos[eDR];
        ePos[eDR] = tmp2;
    }
}

void CompactCube::rotL(int amount) {
    if (amount < 0) amount = 3;
    for (int a = 0; a < amount; a++) {
        Corner tmp = cPos[DLF];
        cPos[DLF] = cPos[UFL];
        cPos[UFL] = cPos[ULB];
        cPos[ULB] = cPos[DBL];
        cPos[DBL] = tmp;
        
        int8_t tOri = cOri[DLF];
        cOri[DLF] = (1 + cOri[UFL]) % 3;
        cOri[UFL] = (2 + cOri[ULB]) % 3;
        cOri[ULB] = (1 + cOri[DBL]) % 3;
        cOri[DBL] = (2 + tOri) % 3;
        
        tOri = eOri[eBL];
        eOri[eBL] = eOri[eDL];
        eOri[eDL] = eOri[eFL];
        eOri[eFL] = eOri[eUL];
        eOri[eUL] = tOri;
        
        Edge tmp2 = ePos[eBL];
        ePos[eBL] = ePos[eDL];
        ePos[eDL] = ePos[eFL];
        ePos[eFL] = ePos[eUL];
        ePos[eUL] = tmp2;
    }
}

void CompactCube::rotR(int amount) {
    if (amount < 0) amount = 3;
    for (int a = 0; a < amount; a++) {
        Corner tmp = cPos[URF];
        cPos[URF] = cPos[DFR];
        cPos[DFR] = cPos[DRB];
        cPos[DRB] = cPos[UBR];
        cPos[UBR] = tmp;
        
        int8_t tOri = cOri[URF];
        cOri[URF] = (1 + cOri[DFR]) % 3;
        cOri[DFR] = (2 + cOri[DRB]) % 3;
        cOri[DRB] = (1 + cOri[UBR]) % 3;
        cOri[UBR] = (2 + tOri) % 3;
        
        tOri = eOri[eFR];
        eOri[eFR] = eOri[eDR];
        eOri[eDR] = eOri[eBR];
        eOri[eBR] = eOri[eUR];
        eOri[eUR] = tOri;
        
        Edge tmp2 = ePos[eFR];
        ePos[eFR] = ePos[eDR];
        ePos[eDR] = ePos[eBR];
        ePos[eBR] = ePos[eUR];
        ePos[eUR] = tmp2;
    }
}

void CompactCube::rotF(int amount) {
    if (amount < 0) amount = 3;
    for (int a = 0; a < amount; a++) {
        Corner tmp = cPos[URF];
        cPos[URF] = cPos[UFL];
        cPos[UFL] = cPos[DLF];
        cPos[DLF] = cPos[DFR];
        cPos[DFR] = tmp;
        
        int8_t tOri = cOri[URF];
        cOri[URF] = (2 + cOri[UFL]) % 3;
        cOri[UFL] = (1 + cOri[DLF]) % 3;
        cOri[DLF] = (2 + cOri[DFR]) % 3;
        cOri[DFR] = (1 + tOri) % 3;
        
        // With orientation flip
        tOri = eOri[eUF];
        eOri[eUF] = 1 - eOri[eFL];
        eOri[eFL] = 1 - eOri[eDF];
        eOri[eDF] = 1 - eOri[eFR];
        eOri[eFR] = 1 - tOri;
        
        Edge tmp2 = ePos[eUF];
        ePos[eUF] = ePos[eFL];
        ePos[eFL] = ePos[eDF];
        ePos[eDF] = ePos[eFR];
        ePos[eFR] = tmp2;
    }
}

void CompactCube::rotB(int amount) {
    if (amount < 0) amount = 3;
    for (int a = 0; a < amount; a++) {
        // Reference: ulb <- ubr <- drb <- dbl <- ulb
        Corner tmp = cPos[ULB];
        cPos[ULB] = cPos[UBR];
        cPos[UBR] = cPos[DRB];
        cPos[DRB] = cPos[DBL];
        cPos[DBL] = tmp;
        
        int8_t tOri = cOri[ULB];
        cOri[ULB] = (2 + cOri[UBR]) % 3;
        cOri[UBR] = (1 + cOri[DRB]) % 3;
        cOri[DRB] = (2 + cOri[DBL]) % 3;
        cOri[DBL] = (1 + tOri) % 3;
        
        // With orientation flip
        tOri = eOri[eBR];
        eOri[eBR] = 1 - eOri[eDB];
        eOri[eDB] = 1 - eOri[eBL];
        eOri[eBL] = 1 - eOri[eUB];
        eOri[eUB] = 1 - tOri;
        
        Edge tmp2 = ePos[eBR];
        ePos[eBR] = ePos[eDB];
        ePos[eDB] = ePos[eBL];
        ePos[eBL] = ePos[eUB];
        ePos[eUB] = tmp2;
    }
}

void CompactCube::applyMove(char face, int amount) {
    switch (face) {
        case 'U': rotU(amount); break;
        case 'D': rotD(amount); break;
        case 'L': rotL(amount); break;
        case 'R': rotR(amount); break;
        case 'F': rotF(amount); break;
        case 'B': rotB(amount); break;
    }
}


Solver::Solver(RubiksCube& c, const std::string& cacheDir) : cube(c), cacheDirectory(cacheDir) {
    resetAllowedMoves();
    
    CompactCube solved;
    for (int i = 1; i <= 4; i++) {
        phaseGoal[i] = getPhaseId(solved, i);
    }
    
    if (!loadDatabaseFiles()) {
        std::cout << "Warning: Could not load all database files. Solver may be slow.\n";
    }
}

void Solver::resetAllowedMoves() {
    for (int i = 0; i < 18; i++) {
        allowedMoves[i] = true;
    }
    currentPhase = 1;
}

void Solver::nextPhase() {
    // Disable moves based on phase transition
    // Moves indexed as: F1,F2,F3,R1,R2,R3,U1,U2,U3,B1,B2,B3,L1,L2,L3,D1,D2,D3
    switch (currentPhase) {
        case 1:
            // Disable F, F', B, B' (keep F2, B2)
            allowedMoves[0] = false;  // F1
            allowedMoves[2] = false;  // F3
            allowedMoves[9] = false;  // B1
            allowedMoves[11] = false; // B3
            break;
        case 2:
            // Also disable R, R', L, L' (keep R2, L2)
            allowedMoves[3] = false;  // R1
            allowedMoves[5] = false;  // R3
            allowedMoves[12] = false; // L1
            allowedMoves[14] = false; // L3
            break;
        case 3:
            // Also disable U, U', D, D' (keep U2, D2)
            allowedMoves[6] = false;  // U1
            allowedMoves[8] = false;  // U3
            allowedMoves[15] = false; // D1
            allowedMoves[17] = false; // D3
            break;
    }
    currentPhase++;
}

// Phase ID encoding functions
int64_t Solver::idPhase1(const CompactCube& c) {
    int64_t id = 0;
    for (int i = 0; i < 12; i++) {
        id <<= 1;
        id += c.eOri[i];
    }
    return id;
}

int64_t Solver::idPhase2(const CompactCube& c) {
    int64_t id = 0;
    for (int i = 0; i < 8; i++) {
        id <<= 2;
        id += c.cOri[i];
    }
    for (int i = 0; i < 12; i++) {
        id <<= 2;
        if (c.ePos[i] < 8)  // Edge is in U/D layer
            id++;
    }
    return id;
}

int64_t Solver::idPhase3(const CompactCube& c) {
    int64_t id = 0;
    
    // Check corners
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 3; j++) {
            id <<= 1;
            char t = cornerNames[c.cPos[i]][(c.cOri[i] + j) % 3];
            char expected = cornerNames[i][j];
            char opposite = faces[(faces.find(expected) + 3) % 6];
            if (!(t == expected || t == opposite)) {
                id++;
            }
        }
    }
    
    // Check edges
    for (int i = 0; i < 11; i++) {
        for (int j = 0; j < 2; j++) {
            id <<= 1;
            char t = edgeNames[c.ePos[i]][(c.eOri[i] + j) % 2];
            char expected = edgeNames[i][j];
            char opposite = faces[(faces.find(expected) + 3) % 6];
            if (!(t == expected || t == opposite)) {
                id++;
            }
        }
    }
    
    // Corner tetrad check
    for (int i = 0; i < 8; i++) {
        id <<= 1;
        if (c.cPos[i] % 4 != i % 4)
            id++;
    }
    
    // Corner permutation parity
    id <<= 1;
    for (int i = 0; i < 8; i++) {
        for (int j = i + 1; j < 8; j++) {
            id ^= (c.cPos[i] > c.cPos[j]);
        }
    }
    
    return id;
}

int64_t Solver::idPhase4(const CompactCube& c) {
    int64_t id = 0;
    
    // Check if each corner/edge sticker matches or is opposite
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 3; j++) {
            id <<= 1;
            char t = cornerNames[c.cPos[i]][(c.cOri[i] + j) % 3];
            char expected = cornerNames[i][j];
            char opposite = faces[(faces.find(expected) + 3) % 6];
            if (t == opposite) {
                id++;
            }
        }
    }
    
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 2; j++) {
            id <<= 1;
            char t = edgeNames[c.ePos[i]][(c.eOri[i] + j) % 2];
            char expected = edgeNames[i][j];
            char opposite = faces[(faces.find(expected) + 3) % 6];
            if (t == opposite) {
                id++;
            }
        }
    }
    
    return id;
}

int64_t Solver::getPhaseId(const CompactCube& c, int phase) {
    switch (phase) {
        case 1: return idPhase1(c);
        case 2: return idPhase2(c);
        case 3: return idPhase3(c);
        case 4: return idPhase4(c);
        default: return 0;
    }
}

bool Solver::loadDatabaseFiles() {
    std::string files[4] = {
        cacheDirectory + "/" + PHASE1_DB,
        cacheDirectory + "/" + PHASE2_DB,
        cacheDirectory + "/" + PHASE3_DB,
        cacheDirectory + "/" + PHASE4_DB
    };
    
    bool allLoaded = true;
    for (int phase = 0; phase < 4; phase++) {
        std::ifstream file(files[phase]);
        if (!file.good()) {
            std::cout << "Database file not found: " << files[phase] << "\n";
            allLoaded = false;
            continue;
        }
        
        int64_t hash;
        std::string moves;
        int count = 0;
        while (file >> hash >> moves) {
            phaseTable[phase][hash] = moves;
            count++;
        }
        std::cout << "Loaded phase " << (phase + 1) << " table: " << count << " entries\n";
    }
    
    tablesLoaded = allLoaded;
    return allLoaded;
}

std::string Solver::lookupSolution(const CompactCube& c, int phase) {
    int64_t id = getPhaseId(c, phase);
    auto it = phaseTable[phase - 1].find(id);
    if (it != phaseTable[phase - 1].end()) {
        return it->second;
    }
    return "";  // Not found
}

std::vector<Move> Solver::parseMoveString(const std::string& moveStr) {
    std::vector<Move> result;
    
    for (size_t i = 0; i + 1 < moveStr.length(); i += 2) {
        char face = moveStr[i];
        int amount = moveStr[i + 1] - '0';
        
        Move m = MOVE_COUNT;
        // Database encoding: 1=CW, 2=180°, 3=CCW (three CW turns)
        auto mapAmounts = [&](Move cw, Move dbl, Move prime) {
            switch (amount) {
                case 1: return cw;      // 1 CW quarter-turn
                case 2: return dbl;     // 180° turn
                default: return prime;  // 3 CW turns = CCW
            }
        };
        switch (face) {
            case 'U': m = mapAmounts(U, U2, U_PRIME); break;
            case 'D': m = mapAmounts(D, D2, D_PRIME); break;
            case 'L': m = mapAmounts(L, L2, L_PRIME); break;
            case 'R': m = mapAmounts(R, R2, R_PRIME); break;
            case 'F': m = mapAmounts(F, F2, F_PRIME); break;
            case 'B': m = mapAmounts(B, B2, B_PRIME); break;
        }
        
        if (m != MOVE_COUNT) {
            result.push_back(m);
        }
    }
    
    return result;
}

std::vector<Move> Solver::solveWithTables(CompactCube startCube) {
    std::vector<Move> solution;
    resetAllowedMoves();
    
    CompactCube current = startCube;
    
    // Debug: print initial compact cube state
    std::cout << "Initial compact state:\n";
    std::cout << "  cPos: ";
    for (int i = 0; i < 8; i++) std::cout << (int)current.cPos[i] << " ";
    std::cout << "\n  cOri: ";
    for (int i = 0; i < 8; i++) std::cout << (int)current.cOri[i] << " ";
    std::cout << "\n  ePos: ";
    for (int i = 0; i < 12; i++) std::cout << (int)current.ePos[i] << " ";
    std::cout << "\n  eOri: ";
    for (int i = 0; i < 12; i++) std::cout << (int)current.eOri[i] << " ";
    std::cout << "\n";
    
    for (int phase = 1; phase <= 4; phase++) {
        int64_t currentId = getPhaseId(current, phase);
        
        if (currentId == phaseGoal[phase]) {
            std::cout << "Phase " << phase << ": Already at goal\n";
            nextPhase();
            continue;
        }
        
        std::cout << "Phase " << phase << ": Starting (id=" << currentId << ", goal=" << phaseGoal[phase] << ")\n";
        
        // Follow table steps until this phase hits the goal
        int lookupCount = 0;
        const int MAX_LOOKUPS = 100;  // Safety limit
        
        while (currentId != phaseGoal[phase] && lookupCount < MAX_LOOKUPS) {
            std::string movesStr = lookupSolution(current, phase);
            
            if (movesStr.empty()) {
                std::cout << "Phase " << phase << ": Not in table at id=" << currentId << ", doing BFS...\n";
                break;  // Exit lookup loop, fall through to BFS
            }
            
            if (movesStr == "E") {
                // Entry denotes finished state (should be rare)
                break;
            }
            
            lookupCount++;
            std::cout << "  Lookup " << lookupCount << ": " << movesStr << "\n";
            
            // Apply encoded moves to the compact cube and mirror them in the solution list
            for (size_t i = 0; i + 1 < movesStr.length(); i += 2) {
                char face = movesStr[i];
                int amount = movesStr[i + 1] - '0';  // 1, 2, or 3 CW quarter turns

                current.applyMove(face, amount);
                
                auto parsed = parseMoveString(movesStr.substr(i, 2));
                if (!parsed.empty()) {
                    solution.push_back(parsed[0]);
                }
            }
            
            // Refresh ID for the next lookup iteration
            currentId = getPhaseId(current, phase);
        }
        
        // Goal reached via lookup?
        if (currentId == phaseGoal[phase]) {
            std::cout << "Phase " << phase << ": Reached goal via " << lookupCount << " lookups\n";
            
            // Debug: print state after this phase
            std::cout << "After phase " << phase << ":\n";
            std::cout << "  cPos: ";
            for (int i = 0; i < 8; i++) std::cout << (int)current.cPos[i] << " ";
            std::cout << "\n  cOri: ";
            for (int i = 0; i < 8; i++) std::cout << (int)current.cOri[i] << " ";
            std::cout << "\n  ePos: ";
            for (int i = 0; i < 12; i++) std::cout << (int)current.ePos[i] << " ";
            std::cout << "\n  eOri: ";
            for (int i = 0; i < 12; i++) std::cout << (int)current.eOri[i] << " ";
            std::cout << "\n";
            
            nextPhase();
            continue;
        }
        
        if (lookupCount >= MAX_LOOKUPS) {
            std::cout << "Phase " << phase << ": Exceeded max lookups, trying BFS...\n";
        }
        
        // Level-by-level BFS fallback
        std::queue<CompactCube> currentLevel;
        std::queue<CompactCube> nextLevel;
        std::unordered_set<int64_t> visited;
        currentLevel.push(current);
        visited.insert(currentId);
        
        bool found = false;
        const int MAX_DEPTH = 15;  // Maximum BFS depth
        
        for (int depth = 0; depth <= MAX_DEPTH && !found; depth++) {
            if (currentLevel.empty()) {
                std::cout << "Phase " << phase << ": BFS exhausted at depth " << depth << " (visited " << visited.size() << " states)\n";
                break;
            }
            
            while (!currentLevel.empty() && !found) {
                CompactCube cur = currentLevel.front();
                currentLevel.pop();
                
                int moveIdx = 0;
                for (int move = 0; move < 6; move++) {
                    CompactCube rotated = cur;
                    for (int amount = 0; amount < 3; amount++) {
                        // Apply cumulative quarter turns of this face
                        rotated.applyMove(moves[move], 1);
                        
                        if (allowedMoves[moveIdx]) {
                            int64_t nextId = getPhaseId(rotated, phase);
                            
                            if (visited.find(nextId) == visited.end()) {
                                CompactCube next = rotated;
                                next.path = cur.path;
                                next.path += moves[move];
                                next.path += ('1' + amount);  // 1, 2, or 3 CW quarter turns
                                
                                if (nextId == phaseGoal[phase]) {
                                    // Reached goal via BFS
                                    std::cout << "Phase " << phase << ": BFS found at depth " << (depth + 1) << ": " << next.path << "\n";
                                    auto parsed = parseMoveString(next.path);
                                    for (auto m : parsed) {
                                        solution.push_back(m);
                                    }
                                    current = next;
                                    found = true;
                                    break;
                                }
                                
                                visited.insert(nextId);
                                nextLevel.push(next);
                            }
                        }
                        moveIdx++;
                    }
                    if (found) break;
                }
            }
            
            // Advance BFS frontier
            std::swap(currentLevel, nextLevel);
            while (!nextLevel.empty()) nextLevel.pop();  // Clear queue for reuse
        }
        
        if (!found) {
            std::cout << "Phase " << phase << ": No solution found after " << MAX_DEPTH << " moves! (visited " << visited.size() << " states)\n";
            std::cout << "Cube may be in invalid state - returning partial solution.\n";
            return solution;  // Return partial solution, caller should check
        }
        
        nextPhase();
    }
    
    return solution;
}

CompactCube Solver::stateToCompact(const CubeState& state) {
    return CubeConversion::stateToCompact(state);
}


void Solver::rotateFaceCW(CubeState& state, int face) {
    int base = face * 9;
    std::array<int, 9> temp;
    for (int i = 0; i < 9; i++) temp[i] = state[base + i];
    
    state[base + 0] = temp[6];
    state[base + 1] = temp[3];
    state[base + 2] = temp[0];
    state[base + 3] = temp[7];
    state[base + 4] = temp[4];
    state[base + 5] = temp[1];
    state[base + 6] = temp[8];
    state[base + 7] = temp[5];
    state[base + 8] = temp[2];
}

void Solver::rotateFaceCCW(CubeState& state, int face) {
    rotateFaceCW(state, face);
    rotateFaceCW(state, face);
    rotateFaceCW(state, face);
}

void Solver::rotateFace180(CubeState& state, int face) {
    rotateFaceCW(state, face);
    rotateFaceCW(state, face);
}

CubeState Solver::applyMove(const CubeState& state, Move move) {
    CubeState s = state;
    
    // cycle4(a,b,c,d): a←d, b←a, c←b, d←c  (values rotate: d→a→b→c→d)
    auto cycle4 = [&s](int a, int b, int c, int d) {
        int temp = s[d];
        s[d] = s[c];
        s[c] = s[b];
        s[b] = s[a];
        s[a] = temp;
    };
    
    // Face indices: RIGHT=0, LEFT=1, UP=2, DOWN=3, FRONT=4, BACK=5
    // Sticker indices: face*9 + position (0-8 in reading order)
    //
    // Adjacent face stickers for each move:
    // U: F[0,1,2]=36,37,38  R[0,1,2]=0,1,2  B[0,1,2]=45,46,47  L[0,1,2]=9,10,11
    // D: F[6,7,8]=42,43,44  R[6,7,8]=6,7,8  B[6,7,8]=51,52,53  L[6,7,8]=15,16,17
    // R: U[2,5,8]=20,23,26  F[2,5,8]=38,41,44  D[2,5,8]=29,32,35  B[0,3,6]=45,48,51
    // L: U[0,3,6]=18,21,24  F[0,3,6]=36,39,42  D[0,3,6]=27,30,33  B[2,5,8]=47,50,53
    // F: U[6,7,8]=24,25,26  R[0,3,6]=0,3,6  D[0,1,2]=27,28,29  L[2,5,8]=11,14,17
    // B: U[0,1,2]=18,19,20  R[2,5,8]=2,5,8  D[6,7,8]=33,34,35  L[0,3,6]=9,12,15
    
    switch (move) {
        // U turn viewed from inside top: corners 0→1→4→5, edges 0→1→2→3, so CW/CCW swap
        case Move::U:
            rotateFaceCCW(s, UP);  // CCW because viewing from inside
            // F row 0 → L row 0 → B row 0 → R row 0 → F row 0
            cycle4(9, 45, 0, 36);
            cycle4(10, 46, 1, 37);
            cycle4(11, 47, 2, 38);
            break;
        case Move::U_PRIME:
            rotateFaceCW(s, UP);  // CW because viewing from inside
            // Reverse: F→R→B→L→F
            cycle4(0, 45, 9, 36);
            cycle4(1, 46, 10, 37);
            cycle4(2, 47, 11, 38);
            break;
        case Move::U2:
            s = applyMove(s, Move::U);
            s = applyMove(s, Move::U);
            break;
            
        // D turn viewed from inside bottom: corners 2→3→6→7, edges 4→5→6→7, so CW/CCW swap
        case Move::D:
            rotateFaceCCW(s, DOWN);  // CCW because viewing from inside
            cycle4(6, 51, 15, 42);
            cycle4(7, 52, 16, 43);
            cycle4(8, 53, 17, 44);
            break;
        case Move::D_PRIME:
            rotateFaceCW(s, DOWN);  // CW because viewing from inside
            cycle4(15, 51, 6, 42);
            cycle4(16, 52, 7, 43);
            cycle4(17, 53, 8, 44);
            break;
        case Move::D2:
            s = applyMove(s, Move::D);
            s = applyMove(s, Move::D);
            break;
            
        // R turn from the right: corners 0→3→6→1, edges 1→8→5→9 with corner twists only
        case Move::R: {
            // Save R-face corners URF[20,0,38], UBR[26,45,2], DRB[29,8,51], DFR[35,44,6]
            int t0 = s[20], t1 = s[0], t2 = s[38];     // Save URF
            int u0 = s[26], u1 = s[45], u2 = s[2];     // Save UBR
            int v0 = s[29], v1 = s[8], v2 = s[51];     // Save DRB
            int w0 = s[35], w1 = s[44], w2 = s[6];     // Save DFR
            // Save edges UR[23,1], FR[41,3], DR[32,7], BR[48,5]
            int e0_0 = s[23], e0_1 = s[1];   // UR
            int e1_0 = s[41], e1_1 = s[3];   // FR
            int e2_0 = s[32], e2_1 = s[7];   // DR
            int e3_0 = s[48], e3_1 = s[5];   // BR
            
            rotateFaceCW(s, RIGHT);
            
            // Edge cycle: UR←FR←DR←BR←UR
            s[23] = e1_0; s[1] = e1_1;   // UR ← FR
            s[41] = e2_0; s[3] = e2_1;   // FR ← DR
            s[32] = e3_0; s[7] = e3_1;   // DR ← BR
            s[48] = e0_0; s[5] = e0_1;   // BR ← UR
            
            // Corner cycle with twists: URF←DFR←DRB←UBR←URF
            // URF ← DFR with +1: URF[j] = DFR[(j+2)%3]
            s[20] = w2; s[0] = w0; s[38] = w1;
            // UBR ← URF with +2: UBR[j] = URF[(j+1)%3]
            s[26] = t1; s[45] = t2; s[2] = t0;
            // DRB ← UBR with +1: DRB[j] = UBR[(j+2)%3]
            s[29] = u2; s[8] = u0; s[51] = u1;
            // DFR ← DRB with +2: DFR[j] = DRB[(j+1)%3]
            s[35] = v1; s[44] = v2; s[6] = v0;
            break;
        }
        case Move::R_PRIME: {
            // Save R-face corners and edges before CCW turn
            int t0 = s[20], t1 = s[0], t2 = s[38];
            int u0 = s[26], u1 = s[45], u2 = s[2];
            int v0 = s[29], v1 = s[8], v2 = s[51];
            int w0 = s[35], w1 = s[44], w2 = s[6];
            // Edges UR[23,1], FR[41,3], DR[32,7], BR[48,5]
            int e0_0 = s[23], e0_1 = s[1];   // UR
            int e1_0 = s[41], e1_1 = s[3];   // FR
            int e2_0 = s[32], e2_1 = s[7];   // DR
            int e3_0 = s[48], e3_1 = s[5];   // BR
            
            rotateFaceCCW(s, RIGHT);
            
            // Reverse edge cycle: UR←BR←DR←FR←UR
            s[23] = e3_0; s[1] = e3_1;   // UR ← BR
            s[48] = e2_0; s[5] = e2_1;   // BR ← DR
            s[32] = e1_0; s[7] = e1_1;   // DR ← FR
            s[41] = e0_0; s[3] = e0_1;   // FR ← UR
            
            // Reverse corner cycle: URF←UBR←DRB←DFR←URF with twists +1, +2, +1, +2
            // URF ← UBR with +1: URF[j] = UBR[(j+2)%3]
            s[20] = u2; s[0] = u0; s[38] = u1;
            // UBR ← DRB with +2: UBR[j] = DRB[(j+1)%3]
            s[26] = v1; s[45] = v2; s[2] = v0;
            // DRB ← DFR with +1: DRB[j] = DFR[(j+2)%3]
            s[29] = w2; s[8] = w0; s[51] = w1;
            // DFR ← URF with +2: DFR[j] = URF[(j+1)%3]
            s[35] = t1; s[44] = t2; s[6] = t0;
            break;
        }
        case Move::R2: {
            // Save R-face stickers before 180° turn
            int t0 = s[20], t1 = s[0], t2 = s[38];     // URF
            int u0 = s[26], u1 = s[45], u2 = s[2];     // UBR
            int v0 = s[29], v1 = s[8], v2 = s[51];     // DRB
            int w0 = s[35], w1 = s[44], w2 = s[6];     // DFR
            int e0_0 = s[23], e0_1 = s[1];   // UR
            int e1_0 = s[41], e1_1 = s[3];   // FR
            int e2_0 = s[32], e2_1 = s[7];   // DR
            int e3_0 = s[48], e3_1 = s[5];   // BR
            
            rotateFace180(s, RIGHT);
            
            // Edge swaps: UR↔DR, FR↔BR
            s[23] = e2_0; s[1] = e2_1;   // UR ← DR
            s[32] = e0_0; s[7] = e0_1;   // DR ← UR
            s[41] = e3_0; s[3] = e3_1;   // FR ← BR
            s[48] = e1_0; s[5] = e1_1;   // BR ← FR
            
            // Corner swaps: URF↔DRB, UBR↔DFR
            s[20] = v0; s[0] = v1; s[38] = v2;     // URF ← DRB
            s[29] = t0; s[8] = t1; s[51] = t2;     // DRB ← URF
            s[26] = w0; s[45] = w1; s[2] = w2;     // UBR ← DFR
            s[35] = u0; s[44] = u1; s[6] = u2;     // DFR ← UBR
            break;
        }
            
        // L turn from the left: corners 2→5→4→7 with twists, edges 10→7→11→3
        case Move::L: {
            // Save L-face corners UFL[18,36,11], ULB[24,9,47], DBL[27,53,15], DLF[33,17,42]
            int t0 = s[18], t1 = s[36], t2 = s[11];     // Save UFL
            int u0 = s[24], u1 = s[9], u2 = s[47];      // Save ULB
            int v0 = s[27], v1 = s[53], v2 = s[15];     // Save DBL
            int w0 = s[33], w1 = s[17], w2 = s[42];     // Save DLF
            // Save edges UL[21,10], FL[39,14], DL[30,16], BL[50,12]
            int e0_0 = s[21], e0_1 = s[10];  // UL (edge 3)
            int e1_0 = s[39], e1_1 = s[14];  // FL (edge 11)
            int e2_0 = s[30], e2_1 = s[16];  // DL (edge 7)
            int e3_0 = s[50], e3_1 = s[12];  // BL (edge 10)
            
            rotateFaceCW(s, LEFT);
            
            // Edge cycle: BL←DL←FL←UL←BL (10←7←11←3←10)
            s[50] = e2_0; s[12] = e2_1;  // BL ← DL
            s[30] = e1_0; s[16] = e1_1;  // DL ← FL
            s[39] = e0_0; s[14] = e0_1;  // FL ← UL
            s[21] = e3_0; s[10] = e3_1;  // UL ← BL
            
            // Corner cycle with twists: DLF←UFL←ULB←DBL←DLF
            // DLF ← UFL with +1: DLF[j] = UFL[(j+2)%3]
            s[33] = t2; s[17] = t0; s[42] = t1;
            // UFL ← ULB with +2: UFL[j] = ULB[(j+1)%3]
            s[18] = u1; s[36] = u2; s[11] = u0;
            // ULB ← DBL with +1: ULB[j] = DBL[(j+2)%3]
            s[24] = v2; s[9] = v0; s[47] = v1;
            // DBL ← DLF with +2: DBL[j] = DLF[(j+1)%3]
            s[27] = w1; s[53] = w2; s[15] = w0;
            break;
        }
        case Move::L_PRIME: {
            // Save L-face corners (DLF, UFL, ULB, DBL) and edges before CCW turn
            int t0 = s[33], t1 = s[17], t2 = s[42];     // Save DLF
            int u0 = s[18], u1 = s[36], u2 = s[11];     // Save UFL
            int v0 = s[24], v1 = s[9], v2 = s[47];      // Save ULB
            int w0 = s[27], w1 = s[53], w2 = s[15];     // Save DBL
            // Edges UL[21,10], FL[39,14], DL[30,16], BL[50,12]
            int e0_0 = s[21], e0_1 = s[10];  // UL (edge 3)
            int e1_0 = s[39], e1_1 = s[14];  // FL (edge 11)
            int e2_0 = s[30], e2_1 = s[16];  // DL (edge 7)
            int e3_0 = s[50], e3_1 = s[12];  // BL (edge 10)
            
            rotateFaceCCW(s, LEFT);
            
            // Reverse edge cycle: UL←FL←DL←BL←UL (3←11←7←10←3)
            s[21] = e1_0; s[10] = e1_1;  // UL ← FL
            s[39] = e2_0; s[14] = e2_1;  // FL ← DL
            s[30] = e3_0; s[16] = e3_1;  // DL ← BL
            s[50] = e0_0; s[12] = e0_1;  // BL ← UL
            
            // Reverse corner cycle: DBL←ULB←UFL←DLF←DBL with twists +2,+1,+2,+1
            // DLF ← DBL with +1: DLF[j] = DBL[(j+2)%3]
            s[33] = w2; s[17] = w0; s[42] = w1;
            // DBL ← ULB with +2: DBL[j] = ULB[(j+1)%3]
            s[27] = v1; s[53] = v2; s[15] = v0;
            // ULB ← UFL with +1: ULB[j] = UFL[(j+2)%3]
            s[24] = u2; s[9] = u0; s[47] = u1;
            // UFL ← DLF with +2: UFL[j] = DLF[(j+1)%3]
            s[18] = t1; s[36] = t2; s[11] = t0;
            break;
        }
        case Move::L2: {
            // L2 180° swap—save stickers first
            int t0 = s[18], t1 = s[36], t2 = s[11];    // UFL
            int u0 = s[24], u1 = s[9], u2 = s[47];     // ULB
            int v0 = s[27], v1 = s[53], v2 = s[15];    // DBL
            int w0 = s[33], w1 = s[17], w2 = s[42];    // DLF
            int e0_0 = s[21], e0_1 = s[10];  // UL
            int e1_0 = s[39], e1_1 = s[14];  // FL
            int e2_0 = s[30], e2_1 = s[16];  // DL
            int e3_0 = s[50], e3_1 = s[12];  // BL
            
            rotateFace180(s, LEFT);
            
            // Edge swaps: UL↔DL, FL↔BL
            s[21] = e2_0; s[10] = e2_1;  // UL ← DL
            s[30] = e0_0; s[16] = e0_1;  // DL ← UL
            s[39] = e3_0; s[14] = e3_1;  // FL ← BL
            s[50] = e1_0; s[12] = e1_1;  // BL ← FL
            
            // Corner swaps: UFL↔DBL, ULB↔DLF
            s[18] = v0; s[36] = v1; s[11] = v2;    // UFL ← DBL
            s[27] = t0; s[53] = t1; s[15] = t2;    // DBL ← UFL
            s[24] = w0; s[9] = w1; s[47] = w2;     // ULB ← DLF
            s[33] = u0; s[17] = u1; s[42] = u2;    // DLF ← ULB
            break;
        }
            
        // F turn from the front: corners 0→5→2→3, edges 0→11→4→8 with flips
        case Move::F: {
            // Save F-face corners URF[20,0,38], UFL[18,36,11], DLF[33,17,42], DFR[35,44,6]
            int t0 = s[20], t1 = s[0], t2 = s[38];
            int u0 = s[18], u1 = s[36], u2 = s[11];
            int v0 = s[33], v1 = s[17], v2 = s[42];
            int w0 = s[35], w1 = s[44], w2 = s[6];
            
            // Edge stickers UF[19,37], FL[39,14], DF[34,43], FR[41,3]
            int e0_0 = s[19], e0_1 = s[37];  // UF
            int e1_0 = s[39], e1_1 = s[14];  // FL
            int e2_0 = s[34], e2_1 = s[43];  // DF
            int e3_0 = s[41], e3_1 = s[3];   // FR
            
            rotateFaceCW(s, FRONT);
            
            // Edge cycle with flip: UF←FL←DF←FR←UF
            s[19] = e1_1; s[37] = e1_0;  // UF ← FL flipped
            s[39] = e2_1; s[14] = e2_0;  // FL ← DF flipped
            s[34] = e3_1; s[43] = e3_0;  // DF ← FR flipped
            s[41] = e0_1; s[3] = e0_0;   // FR ← UF flipped
            
            // Corner cycle with twists +2, +1, +2, +1: URF←UFL←DLF←DFR←URF
            // URF ← UFL with +2: URF[j] = UFL[(j+1)%3]
            s[20] = u1; s[0] = u2; s[38] = u0;
            // UFL ← DLF with +1: UFL[j] = DLF[(j+2)%3]
            s[18] = v2; s[36] = v0; s[11] = v1;
            // DLF ← DFR with +2: DLF[j] = DFR[(j+1)%3]
            s[33] = w1; s[17] = w2; s[42] = w0;
            // DFR ← URF with +1: DFR[j] = URF[(j+2)%3]
            s[35] = t2; s[44] = t0; s[6] = t1;
            break;
        }
        case Move::F_PRIME: {
            // Save F-face corners (URF, UFL, DLF, DFR) and matching edges before CCW turn
            int t0 = s[20], t1 = s[0], t2 = s[38];    // URF
            int u0 = s[18], u1 = s[36], u2 = s[11];   // UFL
            int v0 = s[33], v1 = s[17], v2 = s[42];   // DLF
            int w0 = s[35], w1 = s[44], w2 = s[6];    // DFR
            
            // Edge stickers UF[19,37], FL[39,14], DF[34,43], FR[41,3]
            int e0_0 = s[19], e0_1 = s[37];  // UF
            int e1_0 = s[39], e1_1 = s[14];  // FL
            int e2_0 = s[34], e2_1 = s[43];  // DF
            int e3_0 = s[41], e3_1 = s[3];   // FR
            
            rotateFaceCCW(s, FRONT);
            
            // Reverse edge cycle WITH FLIP: UF←FR←DF←FL←UF
            s[19] = e3_1; s[37] = e3_0;  // UF ← FR flipped
            s[41] = e2_1; s[3] = e2_0;   // FR ← DF flipped
            s[34] = e1_1; s[43] = e1_0;  // DF ← FL flipped
            s[39] = e0_1; s[14] = e0_0;  // FL ← UF flipped
            
            // Reverse corner cycle: DFR←DLF←UFL←URF←DFR with twists +2,+1,+2,+1
            // URF ← DFR with +2: URF[j] = DFR[(j+1)%3]
            s[20] = w1; s[0] = w2; s[38] = w0;
            // UFL ← URF with +1: UFL[j] = URF[(j+2)%3]
            s[18] = t2; s[36] = t0; s[11] = t1;
            // DLF ← UFL with +2: DLF[j] = UFL[(j+1)%3]
            s[33] = u1; s[17] = u2; s[42] = u0;
            // DFR ← DLF with +1: DFR[j] = DLF[(j+2)%3]
            s[35] = v2; s[44] = v0; s[6] = v1;
            break;
        }
        case Move::F2: {
            // F2 180° turn—save stickers then swap opposite pieces
            int t0 = s[20], t1 = s[0], t2 = s[38];    // URF
            int u0 = s[18], u1 = s[36], u2 = s[11];   // UFL
            int v0 = s[33], v1 = s[17], v2 = s[42];   // DLF
            int w0 = s[35], w1 = s[44], w2 = s[6];    // DFR
            int e0_0 = s[19], e0_1 = s[37];  // UF
            int e1_0 = s[39], e1_1 = s[14];  // FL
            int e2_0 = s[34], e2_1 = s[43];  // DF
            int e3_0 = s[41], e3_1 = s[3];   // FR
            
            rotateFace180(s, FRONT);
            
            // Edge swaps: UF↔DF, FL↔FR
            s[19] = e2_0; s[37] = e2_1;  // UF ← DF
            s[34] = e0_0; s[43] = e0_1;  // DF ← UF
            s[39] = e3_0; s[14] = e3_1;  // FL ← FR
            s[41] = e1_0; s[3] = e1_1;   // FR ← FL
            
            // Corner swaps: URF↔DLF, UFL↔DFR
            s[20] = v0; s[0] = v1; s[38] = v2;   // URF ← DLF
            s[33] = t0; s[17] = t1; s[42] = t2;  // DLF ← URF
            s[18] = w0; s[36] = w1; s[11] = w2;  // UFL ← DFR
            s[35] = u0; s[44] = u1; s[6] = u2;   // DFR ← UFL
            break;
        }
            
        // B turn from behind: corners 4→1→6→7, edges 9→6→10→2 with flips
        case Move::B: {
            // Save B-face corners ULB[24,9,47], UBR[26,45,2], DRB[29,8,51], DBL[27,53,15]
            int t0 = s[24], t1 = s[9], t2 = s[47];
            int u0 = s[26], u1 = s[45], u2 = s[2];
            int v0 = s[29], v1 = s[8], v2 = s[51];
            int w0 = s[27], w1 = s[53], w2 = s[15];
            
            // Edge stickers UB[25,46], BR[48,5], DB[28,52], BL[50,12]
            int e0_0 = s[25], e0_1 = s[46];  // UB
            int e1_0 = s[48], e1_1 = s[5];   // BR
            int e2_0 = s[28], e2_1 = s[52];  // DB
            int e3_0 = s[50], e3_1 = s[12];  // BL
            
            rotateFaceCW(s, BACK);
            
            // Edge cycle with flip: BR←DB←BL←UB←BR
            s[48] = e2_1; s[5] = e2_0;   // BR ← DB flipped
            s[28] = e3_1; s[52] = e3_0;  // DB ← BL flipped
            s[50] = e0_1; s[12] = e0_0;  // BL ← UB flipped
            s[25] = e1_1; s[46] = e1_0;  // UB ← BR flipped
            
            // Corner cycle with twists +2, +1, +2, +1: ULB←UBR←DRB←DBL←ULB
            // ULB ← UBR with +2: ULB[j] = UBR[(j+1)%3]
            s[24] = u1; s[9] = u2; s[47] = u0;
            // UBR ← DRB with +1: UBR[j] = DRB[(j+2)%3]
            s[26] = v2; s[45] = v0; s[2] = v1;
            // DRB ← DBL with +2: DRB[j] = DBL[(j+1)%3]
            s[29] = w1; s[8] = w2; s[51] = w0;
            // DBL ← ULB with +1: DBL[j] = ULB[(j+2)%3]
            s[27] = t2; s[53] = t0; s[15] = t1;
            break;
        }
        case Move::B_PRIME: {
            // Save B-face corners (ULB, UBR, DRB, DBL) plus UB/BR/DB/BL edges before CCW turn
            int t0 = s[24], t1 = s[9], t2 = s[47];    // ULB
            int u0 = s[26], u1 = s[45], u2 = s[2];    // UBR
            int v0 = s[29], v1 = s[8], v2 = s[51];    // DRB
            int w0 = s[27], w1 = s[53], w2 = s[15];   // DBL
            
            // Edge stickers UB[25,46], BR[48,5], DB[28,52], BL[50,12]
            int e0_0 = s[25], e0_1 = s[46];  // UB
            int e1_0 = s[48], e1_1 = s[5];   // BR
            int e2_0 = s[28], e2_1 = s[52];  // DB
            int e3_0 = s[50], e3_1 = s[12];  // BL
            
            rotateFaceCCW(s, BACK);
            
            // Reverse edge cycle WITH FLIP: BR←UB←BL←DB←BR
            s[48] = e0_1; s[5] = e0_0;   // BR ← UB flipped
            s[25] = e3_1; s[46] = e3_0;  // UB ← BL flipped
            s[50] = e2_1; s[12] = e2_0;  // BL ← DB flipped
            s[28] = e1_1; s[52] = e1_0;  // DB ← BR flipped
            
            // Reverse corner cycle: DBL←DRB←UBR←ULB←DBL with twists +1,+2,+1,+2
            // ULB ← DBL with +2: ULB[j] = DBL[(j+1)%3]
            s[24] = w1; s[9] = w2; s[47] = w0;
            // UBR ← ULB with +1: UBR[j] = ULB[(j+2)%3]
            s[26] = t2; s[45] = t0; s[2] = t1;
            // DRB ← UBR with +2: DRB[j] = UBR[(j+1)%3]
            s[29] = u1; s[8] = u2; s[51] = u0;
            // DBL ← DRB with +1: DBL[j] = DRB[(j+2)%3]
            s[27] = v2; s[53] = v0; s[15] = v1;
            break;
        }
        case Move::B2: {
            // B2 180° swap—save stickers first
            int t0 = s[24], t1 = s[9], t2 = s[47];    // ULB
            int u0 = s[26], u1 = s[45], u2 = s[2];    // UBR
            int v0 = s[29], v1 = s[8], v2 = s[51];    // DRB
            int w0 = s[27], w1 = s[53], w2 = s[15];   // DBL
            int e0_0 = s[25], e0_1 = s[46];  // UB
            int e1_0 = s[48], e1_1 = s[5];   // BR
            int e2_0 = s[28], e2_1 = s[52];  // DB
            int e3_0 = s[50], e3_1 = s[12];  // BL
            
            rotateFace180(s, BACK);
            
            // Edge swaps: UB↔DB, BL↔BR
            s[25] = e2_0; s[46] = e2_1;  // UB ← DB
            s[28] = e0_0; s[52] = e0_1;  // DB ← UB
            s[50] = e1_0; s[12] = e1_1;  // BL ← BR
            s[48] = e3_0; s[5] = e3_1;   // BR ← BL
            
            // Corner swaps: ULB↔DRB, UBR↔DBL
            s[24] = v0; s[9] = v1; s[47] = v2;    // ULB ← DRB
            s[29] = t0; s[8] = t1; s[51] = t2;    // DRB ← ULB
            s[26] = w0; s[45] = w1; s[2] = w2;    // UBR ← DBL
            s[27] = u0; s[53] = u1; s[15] = u2;   // DBL ← UBR
            break;
        }
            
        default:
            break;
    }
    
    return s;
}

bool Solver::isSolved(const CubeState& state) {
    for (int face = 0; face < 6; face++) {
        int center = state[face * 9 + 4];
        for (int i = 0; i < 9; i++) {
            if (state[face * 9 + i] != center) return false;
        }
    }
    return true;
}

std::vector<Move> Solver::solve() {
    const auto& rawState = cube.getState();
    CubeState state;
    std::copy(rawState.begin(), rawState.end(), state.begin());
    return solveFromState(state);
}

std::vector<Move> Solver::solveFromState(const CubeState& stateSnapshot) {
    if (isSolved(stateSnapshot)) {
        std::cout << "Cube is already solved!\n";
        return {};
    }
    
    std::cout << "Converting to compact representation...\n";
    CompactCube compact = stateToCompact(stateSnapshot);
    
    std::cout << "Solving with lookup tables...\n";
    auto solution = solveWithTables(compact);
    
    std::cout << "Solution found: " << solution.size() << " moves\n";
    std::cout << "Solution: ";
    for (auto m : solution) {
        std::cout << moveToString(m) << " ";
    }
    std::cout << "\n";
    
    return solution;
}

void Solver::executeMoves(const std::vector<Move>& moves, float duration) {
    for (Move m : moves) {
        int face = -1;
        float angle = 0.0f;
        
        // GLM uses the right-hand rule. With face normals pointing outward,
        // clockwise turns correspond to negative angles for every face.
        switch (m) {
            case Move::U:       face = 2; angle = -90.0f; break;
            case Move::U_PRIME: face = 2; angle = 90.0f; break;
            case Move::U2:      face = 2; angle = 180.0f; break;
            case Move::D:       face = 3; angle = -90.0f; break;
            case Move::D_PRIME: face = 3; angle = 90.0f; break;
            case Move::D2:      face = 3; angle = 180.0f; break;
            case Move::L:       face = 1; angle = -90.0f; break;
            case Move::L_PRIME: face = 1; angle = 90.0f; break;
            case Move::L2:      face = 1; angle = 180.0f; break;
            case Move::R:       face = 0; angle = -90.0f; break;
            case Move::R_PRIME: face = 0; angle = 90.0f; break;
            case Move::R2:      face = 0; angle = 180.0f; break;
            case Move::F:       face = 4; angle = -90.0f; break;
            case Move::F_PRIME: face = 4; angle = 90.0f; break;
            case Move::F2:      face = 4; angle = 180.0f; break;
            case Move::B:       face = 5; angle = -90.0f; break;
            case Move::B_PRIME: face = 5; angle = 90.0f; break;
            case Move::B2:      face = 5; angle = 180.0f; break;
            default: break;
        }
        
        if (face >= 0) {
            cube.queueFaceRotation(face, angle, duration);
        }
    }
}

std::string Solver::moveToString(Move m) {
    static const char* names[] = {
        "U", "U'", "U2",
        "D", "D'", "D2",
        "L", "L'", "L2",
        "R", "R'", "R2",
        "F", "F'", "F2",
        "B", "B'", "B2"
    };
    if (m >= 0 && m < Move::MOVE_COUNT) {
        return names[m];
    }
    return "?";
}
