#ifndef SOLVER_H
#define SOLVER_H

#include <string>
#include <vector>
#include <unordered_set>
#include <queue>
#include <atomic>
#include "game.h"

// --- FUNCTIONS ---
std::string SerializeState(GameState& s);

// Basic solver. Does BFS search of moves until finding solution
std::string Solve(const GameState& startState, const std::atomic<bool>* cancel = nullptr);

// Solves by minimizing pushes. Now accepts maxIterations (defaulting to 200k if not specified)
std::string SolveOptimized(const GameState& startState, int targetRuleNoun = -1, int targetRuleProp = -1,
                           int maxIterations = 200000, const std::atomic<bool>* cancel = nullptr);

// The Brain - Refactored to Class
class LogicSolver {
public:
    LogicSolver(const GameState& startState);
    std::string NextSolution(const std::atomic<bool>* cancel = nullptr);

private:
    struct LogicNode { 
        GameState state; 
        std::string plan; 
        // Concrete inputs that reproduce this node from the initial state.
        std::string moves;
        // Prevents immediately undoing the previous high-level action.
        std::string lastBrokenRule;
        bool padding = false;
    };
    
    std::queue<LogicNode> q;
    std::unordered_set<std::string> visited;

    std::vector<std::vector<std::string>> foundPlans;
    std::vector<std::string> ParsePlan(const std::string& plan);
    bool IsRedundant(const std::vector<std::string>& currentSteps);
};

#endif
