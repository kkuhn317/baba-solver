#include "../solver.h"
#include "../levels.h"

#include <algorithm>
#include <atomic>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

static void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

static void RequireDerivedStateMatchesBoard(const GameState& state) {
    GameState reparsed = state;
    ParseRules(reparsed);

    for (int i = 0; i < 100; ++i) {
        Require(reparsed.propertyMap[i] == state.propertyMap[i],
                "propertyMap disagrees with the text on the board at element " + std::to_string(i));
    }

    auto expectedTransforms = reparsed.transformRules;
    auto actualTransforms = state.transformRules;
    std::sort(expectedTransforms.begin(), expectedTransforms.end());
    std::sort(actualTransforms.begin(), actualTransforms.end());
    Require(expectedTransforms == actualTransforms,
            "transformRules disagree with the text on the board");
}

static GameState ReplayMoves(GameState state, const std::string& moves) {
    for (char move : moves) {
        int dx = 0;
        int dy = 0;
        if (move == 'L') dx = -1;
        else if (move == 'R') dx = 1;
        else if (move == 'U') dy = -1;
        else if (move == 'D') dy = 1;
        else throw std::runtime_error(std::string("unexpected move character: ") + move);

        state = MakeMove(state, dx, dy);
        RequireDerivedStateMatchesBoard(state);
    }
    return state;
}

static std::string ExtractMoves(const std::string& solution) {
    const std::string marker = "MOVES: ";
    size_t start = solution.rfind(marker);
    Require(start != std::string::npos, "logic solution did not include a replayable MOVES field");
    start += marker.size();
    size_t end = solution.find('\n', start);
    return solution.substr(start, end == std::string::npos ? end : end - start);
}

static void TestEveryGeneratedStateCanBeReparsed() {
    LoadLevel(0, nullptr);
    GameState state = currentState;
    RequireDerivedStateMatchesBoard(state);

    // Exercise walking, pushing, and blocked inputs while checking the central invariant.
    state = ReplayMoves(state, "RRRRDDDDLLLLUUUURRDDLL");
    RequireDerivedStateMatchesBoard(state);
}

static void TestLogicSolutionReplaysToAWin() {
    LoadLevel(0, nullptr);
    GameState start = currentState;
    LogicSolver solver(start);
    std::string solution = solver.NextSolution();

    Require(solution != "No Logic Solution", "logic solver failed the basic level");
    Require(solution != "Logic Solver Timeout", "logic solver timed out on the basic level");

    std::string moves = ExtractMoves(solution);
    GameState finalState = ReplayMoves(start, moves);
    Require(finalState.hasWon, "reported logic solution does not replay to a win");
}

static GameState MakeRuleFormationLevel() {
    currentWidth = 8;
    currentHeight = 8;
    GameState state;
    state.grid.resize(currentWidth * currentHeight);

    auto put = [&](int x, int y, int element) {
        GetCell(state, x, y).objects.push_back({element});
    };

    // Active player rule and player.
    put(1, 7, TEXT_BABA);
    put(2, 7, TEXT_IS);
    put(3, 7, TEXT_YOU);
    put(5, 5, BABA);

    // FLAG, IS and WIN must each be legally pushed into row 1.
    put(1, 2, TEXT_FLAG);
    put(2, 3, TEXT_IS);
    put(3, 2, TEXT_WIN);
    put(6, 1, FLAG);

    ParseRules(state);
    CheckWin(state);
    return state;
}

static void TestReverseRuleSearchAndCacheReplay() {
    GameState start = MakeRuleFormationLevel();
    std::string first = SolveOptimized(start, TEXT_FLAG, TEXT_WIN, 200000);
    Require(!first.empty(), "reverse rule search failed to form FLAG IS WIN");
    GameState firstResult = ReplayMoves(start, first);
    Require((firstResult.propertyMap[FLAG] & P_WIN) != 0,
            "reverse rule-search path did not form FLAG IS WIN");

    std::string cached = SolveOptimized(start, TEXT_FLAG, TEXT_WIN, 200000);
    Require(cached == first, "cached rule-search path differs from the validated path");
    GameState cachedResult = ReplayMoves(start, cached);
    Require((cachedResult.propertyMap[FLAG] & P_WIN) != 0,
            "cached rule-search path does not replay to the requested rule");
}

static void TestImpossibleRuleSearchCanBeCached() {
    currentWidth = 3;
    currentHeight = 3;
    GameState start;
    start.grid.resize(9);
    GetCell(start, 0, 0).objects.push_back({TEXT_BABA});
    GetCell(start, 1, 0).objects.push_back({TEXT_IS});
    GetCell(start, 2, 0).objects.push_back({TEXT_YOU});
    GetCell(start, 1, 1).objects.push_back({BABA});
    ParseRules(start);

    Require(SolveOptimized(start, TEXT_FLAG, TEXT_WIN, 1000).empty(),
            "solver formed a rule without the required words");
    Require(SolveOptimized(start, TEXT_FLAG, TEXT_WIN, 1000).empty(),
            "cached impossible-rule result changed");
}

static void TestFormedRuleIsPhysicalAndReplayable() {
    GameState start = MakeRuleFormationLevel();
    Require((start.propertyMap[FLAG] & P_WIN) == 0, "test level unexpectedly starts with FLAG IS WIN");

    LogicSolver solver(start);
    std::string solution = solver.NextSolution();
    Require(solution != "No Logic Solution", "logic solver failed to assemble FLAG IS WIN");
    Require(solution != "Logic Solver Timeout", "logic solver timed out assembling FLAG IS WIN");

    GameState finalState = ReplayMoves(start, ExtractMoves(solution));
    Require(finalState.hasWon, "rule-formation solution does not replay to a win");
    RequireDerivedStateMatchesBoard(finalState);
}

static void TestMultipleYouObjectsReplayTogether() {
    currentWidth = 10;
    currentHeight = 6;
    GameState start;
    start.grid.resize(currentWidth * currentHeight);
    auto put = [&](int x, int y, int element) {
        GetCell(start, x, y).objects.push_back({element});
    };

    put(0, 0, TEXT_BABA); put(1, 0, TEXT_IS); put(2, 0, TEXT_YOU);
    put(0, 1, TEXT_ROCK); put(1, 1, TEXT_IS); put(2, 1, TEXT_YOU);
    put(0, 2, TEXT_FLAG); put(1, 2, TEXT_IS); put(2, 2, TEXT_WIN);
    put(1, 4, BABA);
    put(3, 4, ROCK);
    put(8, 4, FLAG);
    ParseRules(start);
    CheckWin(start);

    LogicSolver solver(start);
    std::string solution = solver.NextSolution();
    Require(solution != "No Logic Solution" && solution != "Logic Solver Timeout",
            "logic solver failed a multiple-YOU walking solution");
    GameState finalState = ReplayMoves(start, ExtractMoves(solution));
    Require(finalState.hasWon, "multiple-YOU solution does not replay with simultaneous movement");
}

static void TestBundledLevelTwoStillSolves() {
    LoadLevel(1, nullptr); // UI level 2 (levels/1.wheredoigo.txt)
    GameState start = currentState;
    LogicSolver solver(start);
    std::string solution = solver.NextSolution();
    Require(solution != "No Logic Solution" && solution != "Logic Solver Timeout",
            "logic solver regressed on bundled level 2");
    Require(solution.find("Break WALL IS STOP\n -> Form WALL IS STOP") == std::string::npos,
            "logic solver immediately rebuilt the rule it had just broken");
    GameState finalState = ReplayMoves(start, ExtractMoves(solution));
    Require(finalState.hasWon, "bundled level 2 solution does not replay to a win");
}

static void TestBundledLevelThreeStillSolves() {
    LoadLevel(2, nullptr);
    GameState start = currentState;
    LogicSolver solver(start);
    std::string solution = solver.NextSolution();
    Require(solution != "No Logic Solution" && solution != "Logic Solver Timeout",
            "logic solver regressed on bundled level 3");
    GameState finalState = ReplayMoves(start, ExtractMoves(solution));
    Require(finalState.hasWon, "bundled level 3 solution does not replay to a win");
}

static void TestVolcanoStillSolves() {
    int volcanoIndex = -1;
    for (size_t i = 0; i < levels.size(); ++i) {
        for (const auto& row : levels[i].layout) {
            if (row.find("#bim#") != std::string::npos) volcanoIndex = static_cast<int>(i);
        }
    }
    Require(volcanoIndex != -1, "could not locate the bundled Volcano level");
    LoadLevel(volcanoIndex, nullptr);
    GameState start = currentState;
    LogicSolver solver(start);
    std::string solution = solver.NextSolution();
    Require(solution != "No Logic Solution" && solution != "Logic Solver Timeout",
            "logic solver failed the bundled Volcano level");
    GameState finalState = ReplayMoves(start, ExtractMoves(solution));
    Require(finalState.hasWon, "Volcano solution does not replay to a win");
}

static void TestCancellationStopsEverySolverMode() {
    LoadLevel(1, nullptr);
    std::atomic<bool> cancelled{true};
    Require(Solve(currentState, &cancelled) == "Solver cancelled",
            "basic solver ignored cancellation");
    Require(SolveOptimized(currentState, -1, -1, 200000, &cancelled) == "Solver cancelled",
            "optimized solver ignored cancellation");
    LogicSolver logic(currentState);
    Require(logic.NextSolution(&cancelled) == "Solver cancelled",
            "logic solver ignored cancellation");
}

static void TestSolverPublishesProgressSnapshots() {
    currentWidth = 1;
    currentHeight = 1;
    GameState state;
    state.grid.resize(1);
    state.hasWon = true;

    int snapshots = 0;
    Solve(state, nullptr, [&](const GameState& preview, const SolverProgress& progress) {
        ++snapshots;
        Require(preview.grid.size() == 1, "progress callback received an invalid board");
        Require(progress.target == "WIN", "progress callback omitted its search target");
    });
    Require(snapshots > 0, "solver did not publish a progress snapshot");
}

static void TestWalkingWinSearchSkipsBlockedTargets() {
    auto runBlockedCase = [](int blockingText, int blockingProperty, const char* propertyName) {
        currentWidth = 5;
        currentHeight = 4;
        GameState state;
        state.grid.resize(currentWidth * currentHeight);
        auto put = [&](int x, int y, int element) {
            GetCell(state, x, y).objects.push_back({element});
        };

        put(0, 0, TEXT_BABA); put(1, 0, TEXT_IS); put(2, 0, TEXT_YOU);
        put(0, 1, TEXT_FLAG); put(1, 1, TEXT_IS); put(2, 1, TEXT_WIN);
        put(0, 2, TEXT_FLAG); put(1, 2, TEXT_IS); put(2, 2, blockingText);
        put(0, 3, BABA);
        put(4, 3, FLAG);
        ParseRules(state);
        CheckWin(state);

        Require((state.propertyMap[FLAG] & P_WIN) != 0, "test FLAG is not WIN");
        Require((state.propertyMap[FLAG] & blockingProperty) != 0,
                std::string("test FLAG is not ") + propertyName);

        bool attemptedWalkingWin = false;
        std::atomic<bool> cancel{false};
        LogicSolver solver(state);
        solver.NextSolution(&cancel, [&](const GameState&, const SolverProgress& progress) {
            if (progress.target == "Reach WIN") attemptedWalkingWin = true;
            if (progress.target != "Logic plan") cancel.store(true);
        });
        Require(!attemptedWalkingWin,
                std::string("walking WIN search ran for a ") + propertyName + " target");
    };

    runBlockedCase(TEXT_PUSH, P_PUSH, "PUSH");
    runBlockedCase(TEXT_STOP, P_STOP, "STOP");
}

static void TestSelfTransformationsRequireTwoWords() {
    LoadLevel(2, nullptr);
    std::map<int, int> nounCounts;
    for (const auto& cell : currentState.grid) {
        for (const auto& object : cell.objects) {
            if (IsNoun(object.element)) nounCounts[object.element]++;
        }
    }

    std::vector<Rule> potential = GetPotentialRules(currentState);
    for (const Rule& rule : potential) {
        if (rule.noun == rule.prop) {
            Require(nounCounts[rule.noun] >= 2,
                    "self-transformation was proposed with only one noun word");
        }
    }
}

static void TestTrappedSelfTransformationIsRejected() {
    int volcanoIndex = -1;
    for (size_t i = 0; i < levels.size(); ++i) {
        for (const std::string& row : levels[i].layout) {
            if (row.find("#bim#") != std::string::npos) volcanoIndex = static_cast<int>(i);
        }
    }
    Require(volcanoIndex >= 0, "could not locate Volcano for transformation test");
    LoadLevel(volcanoIndex, nullptr);
    for (const Rule& rule : GetPotentialRules(currentState)) {
        Require(rule.noun != TEXT_LAVA || rule.prop != TEXT_LAVA,
                "trapped LAVA word was treated as usable for LAVA IS LAVA");
    }
}

int main(int argc, char** argv) {
    try {
        InitLevels();
        Require(!levels.empty(), "no levels were loaded; run tests from the project directory");
        if (argc > 1 && std::string(argv[1]) == "volcano") {
            TestVolcanoStillSolves();
            std::cout << "Volcano regression passed\n";
            return 0;
        }
        if (argc > 1 && std::string(argv[1]) == "progress") {
            TestSolverPublishesProgressSnapshots();
            std::cout << "Progress callback regression passed\n";
            return 0;
        }
        if (argc > 1 && std::string(argv[1]) == "level3") {
            TestBundledLevelThreeStillSolves();
            std::cout << "Level 3 regression passed\n";
            return 0;
        }
        TestEveryGeneratedStateCanBeReparsed();
        TestLogicSolutionReplaysToAWin();
        TestReverseRuleSearchAndCacheReplay();
        TestImpossibleRuleSearchCanBeCached();
        TestFormedRuleIsPhysicalAndReplayable();
        TestMultipleYouObjectsReplayTogether();
        TestBundledLevelTwoStillSolves();
        TestBundledLevelThreeStillSolves();
        TestVolcanoStillSolves();
        TestCancellationStopsEverySolverMode();
        TestSolverPublishesProgressSnapshots();
        TestWalkingWinSearchSkipsBlockedTargets();
        TestSelfTransformationsRequireTwoWords();
        TestTrappedSelfTransformationIsRejected();
        std::cout << "solver tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "solver tests failed: " << error.what() << '\n';
        return 1;
    }
}
