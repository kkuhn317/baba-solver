#ifndef SOLVER_H
#define SOLVER_H

#include <string>
#include <vector>
#include <unordered_set>
#include <queue>
#include <deque>
#include <atomic>
#include <functional>
#include "game.h"

struct SolverProgress {
    std::string target;
    std::string phase;
    int depthOrPushes = -1;
    size_t processed = 0;
    size_t queued = 0;
    size_t visited = 0;
    int heuristic = -1;
};

using SolverProgressCallback = std::function<void(const GameState&, const SolverProgress&)>;

struct Rule {
    int noun;
    int prop;
    std::string ToString() const { return std::to_string(noun) + "-" + std::to_string(prop); }
};

// --- FUNCTIONS ---
std::string SerializeState(GameState& s);
std::vector<Rule> GetPotentialRules(const GameState& state);

// Basic solver. Does BFS search of moves until finding solution
std::string Solve(const GameState& startState, const std::atomic<bool>* cancel = nullptr,
                  const SolverProgressCallback& progress = {});

// Solves by minimizing pushes. Now accepts maxIterations (defaulting to 200k if not specified)
std::string SolveOptimized(const GameState& startState, int targetRuleNoun = -1, int targetRuleProp = -1,
                           int maxIterations = 200000, const std::atomic<bool>* cancel = nullptr,
                           const SolverProgressCallback& progress = {});

// The Brain - Refactored to Class
class LogicSolver {
public:
    LogicSolver(const GameState& startState);
    std::string NextSolution(const std::atomic<bool>* cancel = nullptr,
                             const SolverProgressCallback& progress = {});

private:
    struct LogicNode { 
        GameState state; 
        std::string plan; 
        // Concrete inputs that reproduce this node from the initial state.
        std::string moves;
        // Prevents immediately undoing the previous high-level action.
        std::string lastBrokenRule;
        std::string lastFormedRule;
        bool preferBoardActions = false;
    };
    
    std::deque<LogicNode> q;
    // Lower-priority alternatives saved while an open rule construction is
    // pursued. They are restored only if that committed branch dead-ends.
    std::deque<LogicNode> deferred;
    std::unordered_set<std::string> visited;

    std::vector<std::vector<std::string>> foundPlans;
    std::vector<std::string> ParsePlan(const std::string& plan);
    bool IsRedundant(const std::vector<std::string>& currentSteps);
};

#endif
