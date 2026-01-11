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

// Helpers (Added these so Solver can see them)
int CharToElement(char c);
bool IsNoun(int e);
bool IsProperty(int e);
int TextToElement(int textID);
PropFlags TextToProp(int textID);
bool HasProp(GameState& state, int e, PropFlags f);

// Core Engine
void ParseRules(GameState& state);
void CheckWin(GameState& state);
GameState MakeMove(const GameState& state, int dx, int dy);
std::string SerializeState(GameState& s);
void LoadLevel(int idx, HWND hwnd);

// Movement (Exposed from movement.cpp)
bool CanMove(GameState& state, int x, int y, int dx, int dy);