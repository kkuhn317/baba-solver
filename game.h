#pragma once
#include <vector>
#include <string>
#include <windows.h> 
#include "constants.h"
#include "definitions.h"

// --- NEW DEFINITIONS ---
#ifndef TEXT_DEFEAT
#define TEXT_DEFEAT (TEXT_SINK + 1)
#define C_DEFEAT RGB(139, 0, 0)
#endif

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
extern bool isEditorMode;
extern int editorPaletteIdx;
extern const std::vector<int> editorPalette;

inline Cell& GetCell(GameState& state, int x, int y) {
    return state.grid[y * currentWidth + x];
}

// --- FUNCTIONS ---

// Helper
bool HasProp(GameState& state, int e, PropFlags f);

// Core Engine
void ParseRules(GameState& state);
void ProcessInteractions(GameState& state);
void CheckWin(GameState& state);
GameState MakeMove(const GameState& state, int dx, int dy);
std::string SerializeState(GameState& s);
void LoadLevel(int idx, HWND hwnd);
void ResizeGrid(int newWidth, int newHeight);

// Movement (Exposed from movement.cpp)
bool CanMove(GameState& state, int x, int y, int dx, int dy);