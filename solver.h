#ifndef SOLVER_H
#define SOLVER_H

#include <string>
#include "game.h"

// --- FUNCTIONS ---
std::string SerializeState(GameState& s);
std::string Solve(const GameState& startState);
std::string SolveOptimized(const GameState& startState);

#endif