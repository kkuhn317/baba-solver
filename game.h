#pragma once
#include <vector>
#include <string>
#include <windows.h> 
#include "constants.h"

// --- STRUCTS ---
struct Object { 
    int element; 
};

struct Cell { 
    std::vector<Object> objects; 
};

struct GameState {
    // FLAT GRID: Access via [y * width + x]
    std::vector<Cell> grid; 
    bool hasWon = false;
    int propertyMap[100] = { 0 };
};

// --- GLOBALS ---
extern GameState currentState;
extern std::vector<GameState> undoStack;
extern int currentWidth;
extern int currentHeight;
extern int currentLevelIndex;

inline Cell& GetCell(GameState& state, int x, int y) {
    return state.grid[y * currentWidth + x];
}

// --- FUNCTIONS ---
void ParseRules(GameState& state);
void CheckWin(GameState& state);

// MakeMove returns a new state (for solver)
GameState MakeMove(const GameState& state, int dx, int dy);
std::string SerializeState(GameState& s);
void LoadLevel(int idx, HWND hwnd);
bool HasProp(GameState& state, int e, PropFlags f);