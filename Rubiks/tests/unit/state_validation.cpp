#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <fstream>
#include <sstream>

#include "ai rubiks.h"
#include "CubeConversion.h"
#include "CubeStateMachine.h"

using std::cout;
using std::endl;

struct SeqMove { char face; int amt; };

static Move mapToOurMove(char f, int amt);
static bool compareCompactPair(const CompactCube& a, const CompactCube& b, std::string* msg=nullptr);
static void printCompactCube(const std::string& label, const CompactCube& c);

static std::string seqToString(const std::vector<SeqMove>& seq) {
    std::string s;
    for (auto& m : seq) { s += m.face; s += char('0' + m.amt); }
    return s;
}

static std::vector<SeqMove> parseSeq(const std::string& s) {
    std::vector<SeqMove> out;
    for (size_t i = 0; i + 1 < s.size(); i += 2) out.push_back({s[i], s[i+1]-'0'});
    return out;
}

static std::vector<SeqMove> parseHumanSequence(const std::string& s, std::vector<std::string>* tokensOut=nullptr) {
    std::vector<SeqMove> out;
    std::istringstream iss(s);
    std::string tok;
    while (iss >> tok) {
        if (tok.empty()) continue;
        if (tokensOut) tokensOut->push_back(tok);
        char face = tok[0];
        int amt = 1;
        if (tok.size() >= 2) {
            if (tok[1] == '2') amt = 2;
            else if (tok[1] == '\'' || tok[1] == 'p' || tok[1] == 'P') amt = 3;
        }
        out.push_back({face, amt});
    }
    return out;
}

static std::vector<SeqMove> invertSequence(const std::vector<SeqMove>& seq) {
    std::vector<SeqMove> out;
    out.reserve(seq.size());
    for (auto it = seq.rbegin(); it != seq.rend(); ++it) {
        int amt = it->amt;
        int inv = (amt == 2) ? 2 : (amt == 1 ? 3 : 1);
        out.push_back({it->face, inv});
    }
    return out;
}

static void applySeqMachine(CubeStateMachine& machine, const std::vector<SeqMove>& seq) {
    for (const auto& step : seq) {
        Move mv = mapToOurMove(step.face, step.amt);
        if (mv != Move::MOVE_COUNT) {
            machine.applyMove(mv);
        }
    }
}

static Move mapToOurMove(char f, int amt) {
    auto mapA = [&](Move cw, Move dbl, Move ccw){ return (amt==1?cw:(amt==2?dbl:ccw)); };
    switch (f) {
        case 'U': return mapA(Move::U, Move::U2, Move::U_PRIME);
        case 'D': return mapA(Move::D, Move::D2, Move::D_PRIME);
        case 'L': return mapA(Move::L, Move::L2, Move::L_PRIME);
        case 'R': return mapA(Move::R, Move::R2, Move::R_PRIME);
        case 'F': return mapA(Move::F, Move::F2, Move::F_PRIME);
        case 'B': return mapA(Move::B, Move::B2, Move::B_PRIME);
        default:  return Move::MOVE_COUNT;
    }
}

static void applySeqSticker(CubeState& st, const std::vector<SeqMove>& seq) {
    for (auto& m : seq) {
        Move mv = mapToOurMove(m.face, m.amt);
        if (mv != Move::MOVE_COUNT) st = Solver::applyMove(st, mv);
    }
}

static void applySeqCompact(CompactCube& c, const std::vector<SeqMove>& seq) {
    for (auto& m : seq) c.applyMove(m.face, m.amt);
}

static bool runCubeStateMachineComparison(const std::vector<SeqMove>& seq, const std::string& label) {
    CubeStateMachine machine;
    CompactCube reference;

    for (size_t step = 0; step < seq.size(); ++step) {
        Move mv = mapToOurMove(seq[step].face, seq[step].amt);
        if (mv == Move::MOVE_COUNT) {
            continue;
        }

        machine.applyMove(mv);
        reference.applyMove(seq[step].face, seq[step].amt);

        CubeState visualState = machine.getState();
        CompactCube visualCompact = CubeConversion::stateToCompact(visualState);
        std::string msg;
        if (!compareCompactPair(visualCompact, reference, &msg)) {
            cout << "[CubeStateMachine] divergence in sequence " << label
                 << " at step " << (step + 1) << " (" << seq[step].face << seq[step].amt << ")\n";
            cout << "  Reason: " << msg;
            printCompactCube("Visual", visualCompact);
            printCompactCube("Expected", reference);
            return false;
        }
    }

    // Final sanity: stickers -> compact should still match reference
    CubeState finalState = machine.getState();
    CompactCube reconverted = CubeConversion::stateToCompact(finalState);
    std::string msg;
    if (!compareCompactPair(reconverted, reference, &msg)) {
        cout << "[CubeStateMachine] final state mismatch for sequence " << label << "\n";
        cout << "  Reason: " << msg;
        printCompactCube("Reconverted", reconverted);
        printCompactCube("Reference", reference);
        return false;
    }

    return true;
}

static void testCubeStateMachineAgainstCompact() {
    cout << "\n=== CubeStateMachine vs CompactCube Tests ===\n";
    std::vector<std::string> fixedSeqs = {
        "R1",
        "F1",
        "R1D2L2F2R3U2F2R3",
        "B2U3L2U2R2F2U3F2",
        "B3R1D2L2F2R3U2F2R3B2U3L2U2R2F2U3F2"
    };

    int passed = 0;
    int failed = 0;

    auto trySequence = [&](const std::string& label, const std::vector<SeqMove>& seq) {
        bool ok = runCubeStateMachineComparison(seq, label);
        if (ok) {
            ++passed;
        } else {
            ++failed;
        }
    };

    for (const auto& seqStr : fixedSeqs) {
        trySequence(seqStr, parseSeq(seqStr));
    }

    std::mt19937 rng(24680);
    const char faces[6] = {'U','D','L','R','F','B'};
    std::uniform_int_distribution<int> df(0,5), da(1,3);
    for (int t = 0; t < 5; ++t) {
        std::vector<SeqMove> seq;
        seq.reserve(25);
        for (int i = 0; i < 25; ++i) {
            seq.push_back({faces[df(rng)], da(rng)});
        }
        trySequence("random" + std::to_string(t + 1), seq);
    }

    cout << "CubeStateMachine summary: " << passed << " passed, " << failed << " failed\n";
}

// === Duplicate the mapping used by our Solver::stateToCompact for testing ===
namespace testmaps {
    constexpr std::array<std::pair<int,int>,12> EDGES = {{
        {19,37},{23,1},{25,46},{21,10},{34,43},{32,7},{28,52},{30,16},{41,3},{48,5},{50,12},{39,14}
    }};
    constexpr std::array<std::array<int,3>,8> CORNERS = {{
        std::array<int,3>{20,0,38}, std::array<int,3>{26,45,2}, std::array<int,3>{33,17,42}, std::array<int,3>{35,44,6},
        std::array<int,3>{24,9,47}, std::array<int,3>{18,36,11}, std::array<int,3>{29,8,51}, std::array<int,3>{27,53,15}
    }};
    static const int solvedCornerColors[8][3] = {
        {2,0,4},{2,5,0},{3,1,4},{3,4,0},{2,1,5},{2,4,1},{3,0,5},{3,5,1}
    };
    static const int solvedEdgeColors[12][2] = {
        {2,4},{2,0},{2,5},{2,1},{3,4},{3,0},{3,5},{3,1},{4,0},{5,0},{5,1},{4,1}
    };
}

static CubeState stickerFromCompact(const CompactCube& c) {
    CubeState s{}; for (auto &v : s) v = -1;
    // Set center stickers (centers never move)
    s[4]  = 0;  // R face center
    s[13] = 1;  // L face center
    s[22] = 2;  // U face center
    s[31] = 3;  // D face center
    s[40] = 4;  // F face center
    s[49] = 5;  // B face center
    // corners
    for (int pos = 0; pos < 8; ++pos) {
        int piece = (int)c.cPos[pos]; int ori = (int)c.cOri[pos];
        const int* pc = testmaps::solvedCornerColors[piece];
        for (int j = 0; j < 3; ++j) {
            int color = pc[(j - ori + 3) % 3];
            s[testmaps::CORNERS[pos][j]] = color;
        }
    }
    // edges
    for (int pos = 0; pos < 12; ++pos) {
        int piece = (int)c.ePos[pos]; int ori = (int)c.eOri[pos];
        const int* pe = testmaps::solvedEdgeColors[piece];
        int c0 = pe[(0 - ori + 2) % 2];
        int c1 = pe[(1 - ori + 2) % 2];
        s[testmaps::EDGES[pos].first]  = c0;
        s[testmaps::EDGES[pos].second] = c1;
    }
    return s;
}

// Our own phase-ID encoders for CompactCube (copied from our solver)
static int64_t idPhase1Our(const CompactCube& c) {
    int64_t id = 0;
    for (int i = 0; i < 12; i++) { id <<= 1; id += c.eOri[i]; }
    return id;
}

static int64_t idPhase2Our(const CompactCube& c) {
    int64_t id = 0;
    for (int i = 0; i < 8; i++) { id <<= 2; id += c.cOri[i]; }
    for (int i = 0; i < 12; i++) { id <<= 2; if (c.ePos[i] < 8) id++; }
    return id;
}

static int64_t idPhase3Our(const CompactCube& c) {
    static const std::string faces = "FRUBLD";
    static const std::string cornerNames[8] = {"URF","UBR","DLF","DFR","ULB","UFL","DRB","DBL"};
    static const std::string edgeNames[12] = {"UF","UR","UB","UL","DF","DR","DB","DL","FR","BR","BL","FL"};
    int64_t id = 0;
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 3; j++) {
            id <<= 1;
            char t = cornerNames[c.cPos[i]][(c.cOri[i] + j) % 3];
            char expected = cornerNames[i][j];
            char opposite = faces[(faces.find(expected) + 3) % 6];
            if (!(t == expected || t == opposite)) id++;
        }
    }
    for (int i = 0; i < 11; i++) {
        for (int j = 0; j < 2; j++) {
            id <<= 1;
            char t = edgeNames[c.ePos[i]][(c.eOri[i] + j) % 2];
            char expected = edgeNames[i][j];
            char opposite = faces[(faces.find(expected) + 3) % 6];
            if (!(t == expected || t == opposite)) id++;
        }
    }
    for (int i = 0; i < 8; i++) { id <<= 1; if (c.cPos[i] % 4 != i % 4) id++; }
    id <<= 1;
    for (int i = 0; i < 8; i++) for (int j = i + 1; j < 8; j++) id ^= (c.cPos[i] > c.cPos[j]);
    return id;
}

static int64_t idPhase4Our(const CompactCube& c) {
    static const std::string faces = "FRUBLD";
    static const std::string cornerNames[8] = {"URF","UBR","DLF","DFR","ULB","UFL","DRB","DBL"};
    static const std::string edgeNames[12] = {"UF","UR","UB","UL","DF","DR","DB","DL","FR","BR","BL","FL"};
    int64_t id = 0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 3; j++) {
            id <<= 1;
            char t = cornerNames[c.cPos[i]][(c.cOri[i] + j) % 3];
            char expected = cornerNames[i][j];
            char opposite = faces[(faces.find(expected) + 3) % 6];
            if (t == opposite) id++;
        }
    }
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 2; j++) {
            id <<= 1;
            char t = edgeNames[c.ePos[i]][(c.eOri[i] + j) % 2];
            char expected = edgeNames[i][j];
            char opposite = faces[(faces.find(expected) + 3) % 6];
            if (t == opposite) id++;
        }
    }
    return id;
}

static CubeState solvedSticker() {
    CubeState s{}; for(int f=0;f<6;++f) for(int i=0;i<9;++i) s[f*9+i]=f; return s;
}

static std::unordered_map<int64_t, std::string> gPhaseTable[4];
static bool gTablesLoaded = false;

static bool loadPhaseTables(const std::string& dir) {
    bool ok = true;
    for (int p = 0; p < 4; ++p) {
        std::ifstream in(dir + "/phase" + std::to_string(p + 1));
        if (!in.good()) { ok = false; continue; }
        gPhaseTable[p].clear();
        int64_t h; std::string mv;
        while (in >> h >> mv) gPhaseTable[p][h] = mv;
    }
    gTablesLoaded = ok;
    return ok;
}

static bool solveByTables(CompactCube start, std::string& allMoves, CompactCube& endOut) {
    CompactCube solved;
    int64_t goal[5];
    goal[1] = idPhase1Our(solved);
    goal[2] = idPhase2Our(solved);
    goal[3] = idPhase3Our(solved);
    goal[4] = idPhase4Our(solved);

    CompactCube cur = start;
    allMoves.clear();
    for (int ph = 1; ph <= 4; ++ph) {
        int64_t id = (ph==1? idPhase1Our(cur) : ph==2? idPhase2Our(cur) : ph==3? idPhase3Our(cur) : idPhase4Our(cur));
        if (id == goal[ph]) continue;
        auto it = gPhaseTable[ph-1].find(id);
        if (it == gPhaseTable[ph-1].end()) return false;
        const std::string& mv = it->second;
        if (mv == "E") continue;
        for (size_t i = 0; i + 1 < mv.size(); i += 2) {
            char f = mv[i]; int a = mv[i+1] - '0';
            cur.applyMove(f, a);
            allMoves.push_back(f);
            allMoves.push_back(mv[i+1]);
        }
    }
    endOut = cur;
    return endOut == solved;
}

static void runSolveWithTables(const std::vector<SeqMove>& seq) {
    if (!gTablesLoaded) loadPhaseTables("Rubiks");
    cout << "Tables solve test: " << seqToString(seq) << "\n";
    CompactCube start; applySeqCompact(start, seq);
    std::string allMoves; CompactCube end;
    bool compactSolved = solveByTables(start, allMoves, end);
    cout << "  Compact solved: " << (compactSolved?"YES":"NO");
    CubeState st = stickerFromCompact(start);
    auto parsed = Solver::parseMoveString(allMoves);
    for (auto m : parsed) st = Solver::applyMove(st, m);
    bool stickerSolved = Solver::isSolved(st);
    cout << "; Sticker solved: " << (stickerSolved?"YES":"NO") << "\n";
}

static bool compareCompactPair(const CompactCube& a, const CompactCube& b, std::string* msg) {
    bool ok=true; std::string local;
    for(int i=0;i<8;++i){ if ((int)a.cPos[i] != (int)b.cPos[i]) { ok=false; local += "cPos["+std::to_string(i)+"] mismatch\n"; break; } }
    for(int i=0;i<8 && ok;++i){ if ((int)a.cOri[i] != (int)b.cOri[i]) { ok=false; local += "cOri["+std::to_string(i)+"] mismatch\n"; break; } }
    for(int i=0;i<12 && ok;++i){ if ((int)a.ePos[i] != (int)b.ePos[i]) { ok=false; local += "ePos["+std::to_string(i)+"] mismatch\n"; break; } }
    for(int i=0;i<12 && ok;++i){ if ((int)a.eOri[i] != (int)b.eOri[i]) { ok=false; local += "eOri["+std::to_string(i)+"] mismatch\n"; break; } }
    if (!ok && msg) *msg = local; return ok;
}

static void printCompactCube(const std::string& label, const CompactCube& c) {
    cout << "  " << label << " cPos: ";
    for(int i=0;i<8;++i) cout << (int)c.cPos[i] << " ";
    cout << "\n  " << label << " cOri: ";
    for(int i=0;i<8;++i) cout << (int)c.cOri[i] << " ";
    cout << "\n  " << label << " ePos: ";
    for(int i=0;i<12;++i) cout << (int)c.ePos[i] << " ";
    cout << "\n  " << label << " eOri: ";
    for(int i=0;i<12;++i) cout << (int)c.eOri[i] << " ";
    cout << "\n";
}

static void printStickerCube(const std::string& label, const CubeState& st) {
    const char* faceNames[] = {"R", "L", "U", "D", "F", "B"};
    cout << "  " << label << " stickers:\n";
    for (int face = 0; face < 6; ++face) {
        cout << "    " << faceNames[face] << ": ";
        for (int i = 0; i < 9; ++i) cout << st[face*9 + i] << " ";
        cout << "\n";
    }
}

// Detailed step-by-step debugging for a scramble sequence
static void runDetailedStepDebug(const std::string& scrambleStr) {
    cout << "\n=== Detailed Step Debug for: " << scrambleStr << " ===\n";

    auto seq = parseSeq(scrambleStr);

    // Track both compact and sticker forms in parallel
    CompactCube ccDirect;
    CubeState sticker;
    for (int i = 0; i < 54; ++i) sticker[i] = i / 9;
    
    cout << "\n--- Initial (solved) state ---\n";
    printCompactCube("CompactDirect", ccDirect);
    CompactCube ccFromSticker = CubeConversion::stateToCompact(sticker);
    printCompactCube("FromSticker  ", ccFromSticker);
    
    for (size_t step = 0; step < seq.size(); ++step) {
        char f = seq[step].face;
        int a = seq[step].amt;
        
        cout << "\n--- After move " << (step+1) << ": " << f << a << " ---\n";
        
        ccDirect.applyMove(f, a);
        
        Move mv = mapToOurMove(f, a);
        if (mv != Move::MOVE_COUNT) {
            sticker = Solver::applyMove(sticker, mv);
        }
        
        ccFromSticker = CubeConversion::stateToCompact(sticker);
        
        printCompactCube("CompactDirect", ccDirect);
        printCompactCube("FromSticker  ", ccFromSticker);
        
        std::string msg;
        if (!compareCompactPair(ccDirect, ccFromSticker, &msg)) {
            cout << "  *** DIVERGENCE: " << msg << "\n";
            printStickerCube("Sticker state", sticker);
            return;
        } else {
            cout << "  (match)\n";
        }
    }
    
    cout << "\n--- All " << seq.size() << " scramble moves applied without divergence ---\n";
    
    if (!gTablesLoaded) loadPhaseTables("Rubiks");
    std::string solutionMoves; CompactCube end;
    if (!solveByTables(ccDirect, solutionMoves, end)) {
        cout << "  No table solution found\n";
        return;
    }
    
    cout << "\n--- Applying solution: " << solutionMoves << " ---\n";
    
    for (size_t i = 0; i + 1 < solutionMoves.size(); i += 2) {
        char f = solutionMoves[i];
        int a = solutionMoves[i+1] - '0';
        
        cout << "\n--- Solution step " << ((i/2)+1) << ": " << f << a << " ---\n";
        
        ccDirect.applyMove(f, a);
        
        auto one = Solver::parseMoveString(solutionMoves.substr(i, 2));
        if (!one.empty()) {
            sticker = Solver::applyMove(sticker, one[0]);
        }
        
        ccFromSticker = CubeConversion::stateToCompact(sticker);
        
        printCompactCube("CompactDirect", ccDirect);
        printCompactCube("FromSticker  ", ccFromSticker);
        
        std::string msg;
        if (!compareCompactPair(ccDirect, ccFromSticker, &msg)) {
            cout << "  *** DIVERGENCE: " << msg << "\n";
            printStickerCube("Sticker state", sticker);
            return;
        } else {
            cout << "  (match)\n";
        }
    }
    
    cout << "\n--- Solution complete, checking if solved ---\n";
    bool compactSolved = true;
    for (int i = 0; i < 8 && compactSolved; ++i) {
        if ((int)ccDirect.cPos[i] != i || ccDirect.cOri[i] != 0) compactSolved = false;
    }
    for (int i = 0; i < 12 && compactSolved; ++i) {
        if ((int)ccDirect.ePos[i] != i || ccDirect.eOri[i] != 0) compactSolved = false;
    }
    cout << "  CompactDirect solved: " << (compactSolved ? "YES" : "NO") << "\n";
    
    // Check if sticker is solved
    bool stickerSolved = true;
    for (int face = 0; face < 6 && stickerSolved; ++face) {
        int center = sticker[face * 9 + 4];
        for (int i = 0; i < 9; ++i) {
            if (sticker[face * 9 + i] != center) { stickerSolved = false; break; }
        }
    }
    cout << "  Sticker solved: " << (stickerSolved ? "YES" : "NO") << "\n";
}

static void runTablesStepCheck(const std::vector<SeqMove>& seq) {
    if (!gTablesLoaded) loadPhaseTables("Rubiks");
    CompactCube start; applySeqCompact(start, seq);
    std::string moves; CompactCube end;
    if (!solveByTables(start, moves, end)) { cout << "  StepCheck: table miss\n"; return; }
    CompactCube cc = start;
    CubeState st = stickerFromCompact(start);
    cout << "  StepCheck: " << moves << "\n";
    for (size_t i=0;i+1<moves.size(); i+=2) {
        char f = moves[i]; int a = moves[i+1]-'0';
        
        CubeState stBefore = st;
        CompactCube ccBefore = cc;
        
        cc.applyMove(f,a);
        auto one = Solver::parseMoveString(moves.substr(i,2));
        if (!one.empty()) st = Solver::applyMove(st, one[0]);
        CompactCube ccFromSticker = CubeConversion::stateToCompact(st);
        std::string msg;
        if (!compareCompactPair(cc, ccFromSticker, &msg)) {
            cout << "    Divergence at step " << ((i/2)+1) << " on move " << f << (char)('0'+a) << ": " << msg;
            cout << "    Direct cc cPos: "; for(int j=0;j<8;++j) cout << (int)cc.cPos[j] << " "; cout << "\n";
            cout << "    From sticker cPos: "; for(int j=0;j<8;++j) cout << (int)ccFromSticker.cPos[j] << " "; cout << "\n";
            
            // Print corner sticker values BEFORE move
            cout << "    Corner stickers BEFORE:\n";
            for (int pos = 0; pos < 8; ++pos) {
                std::array<int,3> c;
                for (int j=0;j<3;++j) c[j] = stBefore[testmaps::CORNERS[pos][j]];
                cout << "      pos " << pos << " @ [" << testmaps::CORNERS[pos][0] << "," << testmaps::CORNERS[pos][1] << "," << testmaps::CORNERS[pos][2] << "]: colors=[" << c[0] << "," << c[1] << "," << c[2] << "]";
                cout << " (ccBefore: piece=" << (int)ccBefore.cPos[pos] << " ori=" << (int)ccBefore.cOri[pos] << ")\n";
            }
            
            // Print corner sticker values AFTER move
            cout << "    Corner stickers AFTER:\n";
            for (int pos = 0; pos < 8; ++pos) {
                std::array<int,3> c;
                for (int j=0;j<3;++j) c[j] = st[testmaps::CORNERS[pos][j]];
                cout << "      pos " << pos << " @ [" << testmaps::CORNERS[pos][0] << "," << testmaps::CORNERS[pos][1] << "," << testmaps::CORNERS[pos][2] << "]: colors=[" << c[0] << "," << c[1] << "," << c[2] << "]";
                cout << " (cc: piece=" << (int)cc.cPos[pos] << " ori=" << (int)cc.cOri[pos] << ")\n";
            }
            
            return;
        }
    }
    cout << "    No divergence across steps (Compact==Sticker path)\n";
}

static void runVisualVsCompactReplay(const std::string& label, const std::string& humanMoves) {
    std::vector<std::string> tokens;
    auto solution = parseHumanSequence(humanMoves, &tokens);
    if (solution.empty()) {
        cout << "\n=== Visual vs Compact Replay (" << label << ") skipped: no moves provided ===\n";
        return;
    }

    auto scramble = invertSequence(solution);
    CubeStateMachine visualMachine;
    applySeqMachine(visualMachine, scramble);
    CompactCube expected;
    applySeqCompact(expected, scramble);

    CompactCube initialVisual = CubeConversion::stateToCompact(visualMachine.getState());
    std::string msg;
    if (!compareCompactPair(initialVisual, expected, &msg)) {
        cout << "[Replay] Initial scramble mismatch: " << msg << "\n";
    }

    cout << "\n=== Visual vs Compact Replay (" << label << ") ===\n";
    for (size_t i = 0; i < solution.size(); ++i) {
        Move mv = mapToOurMove(solution[i].face, solution[i].amt);
        if (mv == Move::MOVE_COUNT) {
            cout << "Skipping unknown move token '" << (tokens.size()>i?tokens[i]:"?") << "'\n";
            continue;
        }

        visualMachine.applyMove(mv);
        expected.applyMove(solution[i].face, solution[i].amt);
        CompactCube visual = CubeConversion::stateToCompact(visualMachine.getState());
        bool match = compareCompactPair(visual, expected, &msg);

        cout << "\nStep " << (i + 1) << "/" << solution.size()
             << " Move " << (tokens.size()>i?tokens[i]:std::string(1, solution[i].face))
             << " (" << solution[i].face << solution[i].amt << ")\n";
        printCompactCube("Expected", expected);
        printCompactCube("Visual  ", visual);
        if (!match) {
            cout << "*** mismatch detected: " << msg;
            break;
        } else {
            cout << "  (match)\n";
        }
    }
}

// Test individual moves: compare Solver::applyMove vs CompactCube::applyMove
static void testSingleMoves() {
    cout << "\n=== Testing Individual Moves ===\n";

    const char* moveNames[] = {
        "U1", "U2", "U3", "D1", "D2", "D3",
        "R1", "R2", "R3", "L1", "L2", "L3",
        "F1", "F2", "F3", "B1", "B2", "B3"
    };
    
    struct MoveTest { char face; int amt; Move mv; };
    MoveTest moves[] = {
        {'U', 1, Move::U}, {'U', 2, Move::U2}, {'U', 3, Move::U_PRIME},
        {'D', 1, Move::D}, {'D', 2, Move::D2}, {'D', 3, Move::D_PRIME},
        {'R', 1, Move::R}, {'R', 2, Move::R2}, {'R', 3, Move::R_PRIME},
        {'L', 1, Move::L}, {'L', 2, Move::L2}, {'L', 3, Move::L_PRIME},
        {'F', 1, Move::F}, {'F', 2, Move::F2}, {'F', 3, Move::F_PRIME},
        {'B', 1, Move::B}, {'B', 2, Move::B2}, {'B', 3, Move::B_PRIME}
    };
    
    int passed = 0, failed = 0;
    
    for (int i = 0; i < 18; ++i) {
        CompactCube cc;  // solved
        CubeState st = stickerFromCompact(cc);
        
        cc.applyMove(moves[i].face, moves[i].amt);
        
        st = Solver::applyMove(st, moves[i].mv);
        
        CompactCube ccFromSticker = CubeConversion::stateToCompact(st);
        
        bool match = true;
        std::string diffMsg;
        
        for (int j = 0; j < 8 && match; ++j) {
            if (cc.cPos[j] != ccFromSticker.cPos[j]) {
                match = false;
                diffMsg = "cPos[" + std::to_string(j) + "]: expected " + 
                          std::to_string((int)cc.cPos[j]) + ", got " + 
                          std::to_string((int)ccFromSticker.cPos[j]);
            }
        }
        for (int j = 0; j < 8 && match; ++j) {
            if (cc.cOri[j] != ccFromSticker.cOri[j]) {
                match = false;
                diffMsg = "cOri[" + std::to_string(j) + "]: expected " + 
                          std::to_string((int)cc.cOri[j]) + ", got " + 
                          std::to_string((int)ccFromSticker.cOri[j]);
            }
        }
        for (int j = 0; j < 12 && match; ++j) {
            if (cc.ePos[j] != ccFromSticker.ePos[j]) {
                match = false;
                diffMsg = "ePos[" + std::to_string(j) + "]: expected " + 
                          std::to_string((int)cc.ePos[j]) + ", got " + 
                          std::to_string((int)ccFromSticker.ePos[j]);
            }
        }
        for (int j = 0; j < 12 && match; ++j) {
            if (cc.eOri[j] != ccFromSticker.eOri[j]) {
                match = false;
                diffMsg = "eOri[" + std::to_string(j) + "]: expected " + 
                          std::to_string((int)cc.eOri[j]) + ", got " + 
                          std::to_string((int)ccFromSticker.eOri[j]);
            }
        }
        
        if (match) {
            cout << moveNames[i] << ": OK\n";
            passed++;
        } else {
            cout << moveNames[i] << ": FAIL - " << diffMsg << "\n";
            
            // Print detailed state for debugging
            cout << "  CompactCube cPos: ";
            for (int j = 0; j < 8; ++j) cout << (int)cc.cPos[j] << " ";
            cout << "\n  Sticker->CC cPos: ";
            for (int j = 0; j < 8; ++j) cout << (int)ccFromSticker.cPos[j] << " ";
            cout << "\n  CompactCube cOri: ";
            for (int j = 0; j < 8; ++j) cout << (int)cc.cOri[j] << " ";
            cout << "\n  Sticker->CC cOri: ";
            for (int j = 0; j < 8; ++j) cout << (int)ccFromSticker.cOri[j] << " ";
            cout << "\n  CompactCube ePos: ";
            for (int j = 0; j < 12; ++j) cout << (int)cc.ePos[j] << " ";
            cout << "\n  Sticker->CC ePos: ";
            for (int j = 0; j < 12; ++j) cout << (int)ccFromSticker.ePos[j] << " ";
            cout << "\n  CompactCube eOri: ";
            for (int j = 0; j < 12; ++j) cout << (int)cc.eOri[j] << " ";
            cout << "\n  Sticker->CC eOri: ";
            for (int j = 0; j < 12; ++j) cout << (int)ccFromSticker.eOri[j] << " ";
            cout << "\n";
            
            failed++;
        }
    }
    
    cout << "\nSummary: " << passed << " passed, " << failed << " failed\n";
}

int main() {
    testCubeStateMachineAgainstCompact();
    // First test individual moves
    testSingleMoves();
    
    // Run detailed debug for the simplest failing case
    cout << "\n\n=== Detailed Debug Tests ===\n";
    runDetailedStepDebug("B3B3");  // Test B3 applied twice
    
    cout << "\n\n=== Full Sequence Tests ===\n";
    
    // Fixed sequences
    std::vector<std::string> fixed = {
        "B3",
        "R1D2L2F2R3U2F2R3",
        "B2U3L2U2R2F2U3F2",
        "B3R1D2L2F2R3U2F2R3B2U3L2U2R2F2U3F2" // concat
    };
    for (auto& s : fixed) {
        auto q = parseSeq(s);
        runSolveWithTables(q);
        runTablesStepCheck(q);
    }

    // Random sequences
    std::mt19937 rng(12345);
    const char faces[6] = {'U','D','L','R','F','B'};
    std::uniform_int_distribution<int> df(0,5), da(1,3);
    for (int t=0;t<5;++t){
        std::vector<SeqMove> seq; seq.reserve(20);
        for (int i=0;i<20;++i) seq.push_back({faces[df(rng)], da(rng)});
        runSolveWithTables(seq);
        runTablesStepCheck(seq);
    }

    const std::string observedTail =
        "R2 U' R' L2 U2 F2 D' F2 U2 L2 F2 R2 F2 U' R2 D2 F2 U2 R2 B2 L2 U2 R2 U2 R2 F2";
    runVisualVsCompactReplay("observed_log_tail", observedTail);
    return 0;
}
