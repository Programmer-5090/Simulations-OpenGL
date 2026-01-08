// Rubik's solver diagnostics: rotations, phase IDs, database lookups, and end-to-end solves.

#include <iostream>
#include <cassert>
#include <string>
#include <fstream>
#include "ai rubiks.h"

// Helper to print a CompactCube state
void printCube(const CompactCube& c, const std::string& label) {
    std::cout << label << ":\n";
    std::cout << "  cPos: ";
    for (int i = 0; i < 8; i++) std::cout << (int)c.cPos[i] << " ";
    std::cout << "\n  cOri: ";
    for (int i = 0; i < 8; i++) std::cout << (int)c.cOri[i] << " ";
    std::cout << "\n  ePos: ";
    for (int i = 0; i < 12; i++) std::cout << (int)c.ePos[i] << " ";
    std::cout << "\n  eOri: ";
    for (int i = 0; i < 12; i++) std::cout << (int)c.eOri[i] << " ";
    std::cout << "\n";
}

// Solved cube should emit goal IDs for every phase.
void testSolvedCubePhaseIds() {
    std::cout << "\n=== Test: Solved Cube Phase IDs ===\n";
    
    CompactCube solved;
    
    // Phase 1: edge orientations
    int64_t phase1Id = 0;
    for (int i = 0; i < 12; i++) {
        phase1Id <<= 1;
        phase1Id += solved.eOri[i];
    }
    std::cout << "Phase 1 ID (solved): " << phase1Id << " (expected: 0)\n";
    assert(phase1Id == 0);
    
    // Phase 2: corner orientation + UD slice edges
    int64_t phase2Id = 0;
    for (int i = 0; i < 8; i++) {
        phase2Id <<= 2;
        phase2Id += solved.cOri[i];
    }
    for (int i = 0; i < 12; i++) {
        phase2Id <<= 2;
        if (solved.ePos[i] < 8)  // Edge is in U/D layer
            phase2Id++;
    }
    std::cout << "Phase 2 ID (solved): " << phase2Id << " (this is the goal value)\n";
    
    std::cout << "PASSED: Solved cube phase IDs\n";
}

// Move + inverse must land back on the solved cube.
void testMoveInverse() {
    std::cout << "\n=== Test: Move Inverses ===\n";
    
    char faces[] = {'U', 'D', 'L', 'R', 'F', 'B'};
    
    for (char face : faces) {
        CompactCube c;
        
        // Quarter turn followed by its inverse (1 + 3) -> identity
        c.applyMove(face, 1);
        c.applyMove(face, 3);
        
        CompactCube solved;
        bool match = (c == solved);
        std::cout << "  " << face << "1 + " << face << "3 = identity: " << (match ? "PASS" : "FAIL") << "\n";
        if (!match) {
            printCube(c, "Result");
            printCube(solved, "Expected (solved)");
        }
        assert(match);
        
        // Two 180° turns should also reset the cube
        CompactCube c2;
        c2.applyMove(face, 2);
        c2.applyMove(face, 2);
        match = (c2 == solved);
        std::cout << "  " << face << "2 + " << face << "2 = identity: " << (match ? "PASS" : "FAIL") << "\n";
        assert(match);
    }
    
    std::cout << "PASSED: All move inverses\n";
}

// Phase 1 expects every edge to remain unflipped.
void testPhase1Goal() {
    std::cout << "\n=== Test: Phase 1 Edge Orientations ===\n";
    
    // F/B moves flip the four adjacent edges
    CompactCube c;
    
    // Apply F1 to flip those edges
    c.applyMove('F', 1);
    
    int64_t phase1Id = 0;
    for (int i = 0; i < 12; i++) {
        phase1Id <<= 1;
        phase1Id += c.eOri[i];
    }
    
    std::cout << "After F1, Phase 1 ID: " << phase1Id << "\n";
    std::cout << "Edge orientations: ";
    for (int i = 0; i < 12; i++) std::cout << (int)c.eOri[i] << " ";
    std::cout << "\n";
    
    // F3 undo should return to zero
    c.applyMove('F', 3);
    
    phase1Id = 0;
    for (int i = 0; i < 12; i++) {
        phase1Id <<= 1;
        phase1Id += c.eOri[i];
    }
    
    std::cout << "After F1 F3, Phase 1 ID: " << phase1Id << " (expected: 0)\n";
    assert(phase1Id == 0);
    
    // 180° turns keep orientations
    CompactCube c2;
    c2.applyMove('F', 2);
    
    phase1Id = 0;
    for (int i = 0; i < 12; i++) {
        phase1Id <<= 1;
        phase1Id += c2.eOri[i];
    }
    
    std::cout << "After F2, Phase 1 ID: " << phase1Id << " (expected: 0 for 180° move)\n";
    
    std::cout << "PASSED: Phase 1 edge orientation tests\n";
}

// Quick sanity check for the Phase 4 lookup table.
void testDatabaseLookup() {
    std::cout << "\n=== Test: Database Lookup ===\n";
    
    // Use F2, whose inverse is also F2
    CompactCube c;
    c.applyMove('F', 2);  // F2
    
    std::cout << "After F2:\n";
    printCube(c, "State");
    
    // Manually compute the Phase 4 ID and ensure it matches the DB
    std::string faces = "FRUBLD";
    std::string cornerNames[8] = {"URF", "UBR", "DLF", "DFR", "ULB", "UFL", "DRB", "DBL"};
    std::string edgeNames[12] = {"UF", "UR", "UB", "UL", "DF", "DR", "DB", "DL", "FR", "BR", "BL", "FL"};
    
    int64_t phase4Id = 0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 3; j++) {
            phase4Id <<= 1;
            char t = cornerNames[c.cPos[i]][(c.cOri[i] + j) % 3];
            char expected = cornerNames[i][j];
            char opposite = faces[(faces.find(expected) + 3) % 6];
            if (t == opposite) {
                phase4Id++;
            }
        }
    }
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 2; j++) {
            phase4Id <<= 1;
            char t = edgeNames[c.ePos[i]][(c.eOri[i] + j) % 2];
            char expected = edgeNames[i][j];
            char opposite = faces[(faces.find(expected) + 3) % 6];
            if (t == opposite) {
                phase4Id++;
            }
        }
    }
    
    std::cout << "Phase 4 ID after F2: " << phase4Id << "\n";
    
    std::cout << "Test complete - verify manually against database\n";
}

// Reproduce the user-reported Phase 3 failure.
void testPhase3FailingCase() {
    std::cout << "\n=== Test: Phase 3 Failing Case ===\n";
    
    // Phase 2 snapshot:
    // cPos: 1 0 6 2 3 7 4 5
    // ePos: 0 1 2 5 3 6 4 7 10 8 9 11
    // All orientations are 0
    
    CompactCube c;
    c.cPos[0] = (Corner)1; c.cPos[1] = (Corner)0; c.cPos[2] = (Corner)6; c.cPos[3] = (Corner)2;
    c.cPos[4] = (Corner)3; c.cPos[5] = (Corner)7; c.cPos[6] = (Corner)4; c.cPos[7] = (Corner)5;
    
    c.ePos[0] = (Edge)0; c.ePos[1] = (Edge)1; c.ePos[2] = (Edge)2; c.ePos[3] = (Edge)5;
    c.ePos[4] = (Edge)3; c.ePos[5] = (Edge)6; c.ePos[6] = (Edge)4; c.ePos[7] = (Edge)7;
    c.ePos[8] = (Edge)10; c.ePos[9] = (Edge)8; c.ePos[10] = (Edge)9; c.ePos[11] = (Edge)11;
    
    for (int i = 0; i < 8; i++) c.cOri[i] = 0;
    for (int i = 0; i < 12; i++) c.eOri[i] = 0;
    
    printCube(c, "State after Phase 2");
    
    // Replay the solver path B2R2D1F2U1L2D2F2U3 while tracking IDs
    
    std::string faces = "FRUBLD";
    std::string cornerNames[8] = {"URF", "UBR", "DLF", "DFR", "ULB", "UFL", "DRB", "DBL"};
    std::string edgeNames[12] = {"UF", "UR", "UB", "UL", "DF", "DR", "DB", "DL", "FR", "BR", "BL", "FL"};
    
    auto calcPhase3Id = [&](const CompactCube& cube) {
        int64_t id = 0;
        
        // Check corners
        for (int i = 0; i < 7; i++) {
            for (int j = 0; j < 3; j++) {
                id <<= 1;
                char t = cornerNames[cube.cPos[i]][(cube.cOri[i] + j) % 3];
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
                char t = edgeNames[cube.ePos[i]][(cube.eOri[i] + j) % 2];
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
            if (cube.cPos[i] % 4 != i % 4)
                id++;
        }
        
        // Corner permutation parity
        id <<= 1;
        for (int i = 0; i < 8; i++) {
            for (int j = i + 1; j < 8; j++) {
                id ^= (cube.cPos[i] > cube.cPos[j]);
            }
        }
        
        return id;
    };
    
    int64_t initialId = calcPhase3Id(c);
    std::cout << "Initial Phase 3 ID: " << initialId << "\n";
    
    // Lookup the ID inside phase3 database
    std::cout << "\nLooking up ID " << initialId << " in phase3 database...\n";
    std::ifstream dbFile("Rubiks/assets/phase3");
    if (dbFile.good()) {
        int64_t hash;
        std::string moves;
        bool found = false;
        while (dbFile >> hash >> moves) {
            if (hash == initialId) {
                std::cout << "Database entry: " << hash << " " << moves << "\n";
                found = true;
                break;
            }
        }
        if (!found) {
            std::cout << "ID not found in database!\n";
        }
        dbFile.close();
    } else {
        std::cout << "Could not open database file.\n";
    }
    
    // Apply the recorded move sequence and log each step
    std::string moveStr = "B2R2D1F2U1L2D2F2U3";
    std::cout << "\nApplying moves: " << moveStr << "\n";
    for (size_t i = 0; i + 1 < moveStr.length(); i += 2) {
        char face = moveStr[i];
        int amount = moveStr[i + 1] - '0';
        
        c.applyMove(face, amount);
        
        std::cout << "After " << face << amount << ": Phase 3 ID = " << calcPhase3Id(c) << "\n";
    }
    
    printCube(c, "Final state");
    std::cout << "Final Phase 3 ID: " << calcPhase3Id(c) << " (expected: 0)\n";
    
    // Compare against the solved baseline
    CompactCube solved;
    std::cout << "Solved cube Phase 3 ID: " << calcPhase3Id(solved) << "\n";
}

// Corner tetrads group corners by index mod 4; verify both layouts.
void testPhase3TetradCheck() {
    std::cout << "\n=== Test: Phase 3 Tetrad Check ===\n";
    
    // The tetrad check: cPos[i] % 4 == i % 4
    // This divides corners into two tetrads:
    // Tetrad 0: positions 0, 4 should have corners 0, 4 (URF, ULB)
    // Tetrad 1: positions 1, 5 should have corners 1, 5 (UBR, UFL)
    // Tetrad 2: positions 2, 6 should have corners 2, 6 (DLF, DRB)
    // Tetrad 3: positions 3, 7 should have corners 3, 7 (DFR, DBL)
    
    CompactCube c;
    // Solved baseline
    std::cout << "Solved state tetrads:\n";
    for (int i = 0; i < 8; i++) {
        std::cout << "  Position " << i << ": corner " << (int)c.cPos[i] 
                  << ", pos%4=" << (i % 4) << ", corner%4=" << (c.cPos[i] % 4)
                  << " -> " << ((c.cPos[i] % 4 == i % 4) ? "OK" : "WRONG") << "\n";
    }
    
    // Replay the failing layout: cPos: 0 5 2 1 4 3 6 7
    c.cPos[0] = (Corner)0; c.cPos[1] = (Corner)5; c.cPos[2] = (Corner)2; c.cPos[3] = (Corner)1;
    c.cPos[4] = (Corner)4; c.cPos[5] = (Corner)3; c.cPos[6] = (Corner)6; c.cPos[7] = (Corner)7;
    
    std::cout << "\nFailing state (after Phase 3 moves) tetrads:\n";
    int tetradErrors = 0;
    for (int i = 0; i < 8; i++) {
        bool ok = (c.cPos[i] % 4 == i % 4);
        if (!ok) tetradErrors++;
        std::cout << "  Position " << i << ": corner " << (int)c.cPos[i] 
                  << ", pos%4=" << (i % 4) << ", corner%4=" << (c.cPos[i] % 4)
                  << " -> " << (ok ? "OK" : "WRONG") << "\n";
    }
    std::cout << "Tetrad errors: " << tetradErrors << " (Phase 3 ID will have " << tetradErrors << " bits set)\n";
}

// Confirm that database entries scramble to the advertised IDs.
void testDatabaseMovesForward() {
    std::cout << "\n=== Test: Database Moves (Forward Application) ===\n";
    
    // DB entries describe how to solve a state; running them in reverse from solved should reproduce that state
    
    CompactCube c;
    c.applyMove('F', 2);
    
    std::string faces = "FRUBLD";
    std::string cornerNames[8] = {"URF", "UBR", "DLF", "DFR", "ULB", "UFL", "DRB", "DBL"};
    std::string edgeNames[12] = {"UF", "UR", "UB", "UL", "DF", "DR", "DB", "DL", "FR", "BR", "BL", "FL"};
    
    auto calcPhase3Id = [&](const CompactCube& cube) {
        int64_t id = 0;
        
        for (int i = 0; i < 7; i++) {
            for (int j = 0; j < 3; j++) {
                id <<= 1;
                char t = cornerNames[cube.cPos[i]][(cube.cOri[i] + j) % 3];
                char expected = cornerNames[i][j];
                char opposite = faces[(faces.find(expected) + 3) % 6];
                if (!(t == expected || t == opposite)) {
                    id++;
                }
            }
        }
        
        for (int i = 0; i < 11; i++) {
            for (int j = 0; j < 2; j++) {
                id <<= 1;
                char t = edgeNames[cube.ePos[i]][(cube.eOri[i] + j) % 2];
                char expected = edgeNames[i][j];
                char opposite = faces[(faces.find(expected) + 3) % 6];
                if (!(t == expected || t == opposite)) {
                    id++;
                }
            }
        }
        
        for (int i = 0; i < 8; i++) {
            id <<= 1;
            if (cube.cPos[i] % 4 != i % 4)
                id++;
        }
        
        id <<= 1;
        for (int i = 0; i < 8; i++) {
            for (int j = i + 1; j < 8; j++) {
                id ^= (cube.cPos[i] > cube.cPos[j]);
            }
        }
        
        return id;
    };
    
    std::cout << "After F2, Phase 3 ID: " << calcPhase3Id(c) << " (database says 360 F2)\n";
    
    // Undo and ensure the ID resets
    c.applyMove('F', 2);
    std::cout << "After F2 F2, Phase 3 ID: " << calcPhase3Id(c) << " (expected: 0)\n";
    
    printCube(c, "After F2 F2");
}

// Walk through the iterative lookup logic used during database solves.
void testDatabaseGenerationLogic() {
    std::cout << "\n=== Test: Database Generation Logic ===\n";
    std::cout << "Database stores incremental paths - need multiple lookups to reach goal.\n\n";
    
    std::string faces = "FRUBLD";
    std::string cornerNames[8] = {"URF", "UBR", "DLF", "DFR", "ULB", "UFL", "DRB", "DBL"};
    std::string edgeNames[12] = {"UF", "UR", "UB", "UL", "DF", "DR", "DB", "DL", "FR", "BR", "BL", "FL"};
    
    auto calcPhase3Id = [&](const CompactCube& cube) {
        int64_t id = 0;
        
        for (int i = 0; i < 7; i++) {
            for (int j = 0; j < 3; j++) {
                id <<= 1;
                char t = cornerNames[cube.cPos[i]][(cube.cOri[i] + j) % 3];
                char expected = cornerNames[i][j];
                char opposite = faces[(faces.find(expected) + 3) % 6];
                if (!(t == expected || t == opposite)) {
                    id++;
                }
            }
        }
        
        for (int i = 0; i < 11; i++) {
            for (int j = 0; j < 2; j++) {
                id <<= 1;
                char t = edgeNames[cube.ePos[i]][(cube.eOri[i] + j) % 2];
                char expected = edgeNames[i][j];
                char opposite = faces[(faces.find(expected) + 3) % 6];
                if (!(t == expected || t == opposite)) {
                    id++;
                }
            }
        }
        
        for (int i = 0; i < 8; i++) {
            id <<= 1;
            if (cube.cPos[i] % 4 != i % 4)
                id++;
        }
        
        id <<= 1;
        for (int i = 0; i < 8; i++) {
            for (int j = i + 1; j < 8; j++) {
                id ^= (cube.cPos[i] > cube.cPos[j]);
            }
        }
        
        return id;
    };
    
    // Iterate lookups until the state hits the goal
    CompactCube c;
    c.cPos[0] = (Corner)1; c.cPos[1] = (Corner)0; c.cPos[2] = (Corner)6; c.cPos[3] = (Corner)2;
    c.cPos[4] = (Corner)3; c.cPos[5] = (Corner)7; c.cPos[6] = (Corner)4; c.cPos[7] = (Corner)5;
    
    c.ePos[0] = (Edge)0; c.ePos[1] = (Edge)1; c.ePos[2] = (Edge)2; c.ePos[3] = (Edge)5;
    c.ePos[4] = (Edge)3; c.ePos[5] = (Edge)6; c.ePos[6] = (Edge)4; c.ePos[7] = (Edge)7;
    c.ePos[8] = (Edge)10; c.ePos[9] = (Edge)8; c.ePos[10] = (Edge)9; c.ePos[11] = (Edge)11;
    
    for (int i = 0; i < 8; i++) c.cOri[i] = 0;
    for (int i = 0; i < 12; i++) c.eOri[i] = 0;
    
    std::cout << "Testing iterative lookup on failing state...\n";
    std::cout << "Initial Phase 3 ID: " << calcPhase3Id(c) << "\n\n";
    
    // Load phase3 database
    std::unordered_map<int64_t, std::string> phase3DB;
    std::ifstream dbFile("Rubiks/assets/phase3");
    if (dbFile.good()) {
        int64_t hash;
        std::string moves;
        while (dbFile >> hash >> moves) {
            phase3DB[hash] = moves;
        }
        std::cout << "Loaded " << phase3DB.size() << " Phase 3 entries\n\n";
    } else {
        std::cout << "Could not load phase3 database\n";
        return;
    }
    
    // Iteratively look up and apply
    int iteration = 0;
    const int MAX_ITERATIONS = 20;
    while (calcPhase3Id(c) != 0 && iteration < MAX_ITERATIONS) {
        int64_t id = calcPhase3Id(c);
        auto it = phase3DB.find(id);
        if (it == phase3DB.end()) {
            std::cout << "ID " << id << " not found in database!\n";
            break;
        }
        
        std::string moves = it->second;
        std::cout << "Iteration " << (++iteration) << ": ID=" << id << " -> apply " << moves << "\n";
        
        if (moves == "E") {
            std::cout << "Already at goal!\n";
            break;
        }
        
        // Apply moves
        for (size_t i = 0; i + 1 < moves.length(); i += 2) {
            char face = moves[i];
            int amount = moves[i + 1] - '0';
            c.applyMove(face, amount);
        }
    }
    
    std::cout << "\nFinal Phase 3 ID: " << calcPhase3Id(c) << " (expected: 0)\n";
    if (calcPhase3Id(c) == 0) {
        std::cout << "SUCCESS: Iterative lookup reached goal!\n";
    } else if (iteration >= MAX_ITERATIONS) {
        std::cout << "FAILED: Exceeded max iterations\n";
    }
}

int main() {
    std::cout << "=================================\n";
    std::cout << "Rubik's Cube Solver Tests\n";
    std::cout << "=================================\n";
    
    testSolvedCubePhaseIds();
    testMoveInverse();
    testPhase1Goal();
    testDatabaseLookup();
    testPhase3FailingCase();
    testPhase3TetradCheck();
    testDatabaseMovesForward();
    testDatabaseGenerationLogic();
    
    std::cout << "\n=================================\n";
    std::cout << "All tests completed!\n";
    std::cout << "=================================\n";
    
    return 0;
}
