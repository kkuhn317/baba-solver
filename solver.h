#ifndef SOLVER_H
#define SOLVER_H

#include <string>
#include <vector>
#include <unordered_set>
#include "game.h"

// --- FUNCTIONS ---
std::string SerializeState(GameState& s);

// Basic solver. Does BFS search of moves until finding solution
std::string Solve(const GameState& startState);

// Solves by minimizing pushes. Now accepts maxIterations (defaulting to 200k if not specified)
std::string SolveOptimized(const GameState& startState, int targetRuleNoun = -1, int targetRuleProp = -1, int maxIterations = 200000);

// The Brain
std::string SolveLogic(const GameState& startState, const std::unordered_set<std::string>& forbidden = {});

#endif