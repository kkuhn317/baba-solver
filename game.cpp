#include "game.h"
#include "definitions.h"
#include "movement.h"
#include "levels.h"
#include <algorithm>
#include <tuple>
#include <iostream>

// --- GLOBALS ---
GameState currentState;
std::vector<GameState> undoStack;
int currentWidth = 20;
int currentHeight = 12;
int currentLevelIndex = 0;
bool isEditorMode = false;
int editorPaletteIdx = 0;
const std::vector<int> editorPalette = {
    WALL, BABA, FLAG, ROCK, WATER, SKULL,
    TEXT_BABA, TEXT_FLAG, TEXT_WALL, TEXT_ROCK, TEXT_WATER, TEXT_SKULL,
    TEXT_IS, TEXT_YOU, TEXT_WIN, TEXT_STOP, TEXT_PUSH, TEXT_SINK, TEXT_DEFEAT
};

// --- RULE PARSER ---
void ParseRules(GameState& state) {
    for (int i = 0; i < 100; i++) state.propertyMap[i] = 0; 
    
    auto CheckRule = [&](int n, int i, int p) {
        if (IsNoun(n) && i == TEXT_IS && IsProperty(p)) {
            state.propertyMap[TextToElement(n)] |= TextToProp(p);
        }
    };

    // Horizontal
    for (int y = 0; y < currentHeight; y++) {
        for (int x = 0; x < currentWidth - 2; x++) {
            Cell& midCell = GetCell(state, x+1, y);
            if (midCell.objects.empty()) continue;
            
            int mid = midCell.objects[0].element;
            if (mid != TEXT_IS) continue;

            Cell& left = GetCell(state, x, y);
            Cell& right = GetCell(state, x+2, y);

            if (!left.objects.empty() && !right.objects.empty()) {
                CheckRule(left.objects[0].element, mid, right.objects[0].element);
            }
        }
    }
    // Vertical
    for (int x = 0; x < currentWidth; x++) {
        for (int y = 0; y < currentHeight - 2; y++) {
            Cell& midCell = GetCell(state, x, y+1);
            if (midCell.objects.empty()) continue;
            
            int mid = midCell.objects[0].element;
            if (mid != TEXT_IS) continue;

            Cell& up = GetCell(state, x, y);
            Cell& down = GetCell(state, x, y+2);

            if (!up.objects.empty() && !down.objects.empty()) {
                CheckRule(up.objects[0].element, mid, down.objects[0].element);
            }
        }
    }
}

bool HasProp(GameState& state, int e, PropFlags f) {
    if (e >= TEXT_BABA) return (f == P_PUSH);
    return (state.propertyMap[e] & f);
}

// --- INTERACTIONS (SINK, etc.) ---
void ProcessInteractions(GameState& state) {
    for(auto& cell : state.grid) {
        if(cell.objects.size() < 2) continue;
        
        bool changed = true;
        while(changed) {
            changed = false;

            // DEFEAT Logic
            bool hasDefeat = false;
            for(const auto& o : cell.objects) {
                if(HasProp(state, o.element, P_DEFEAT)) { hasDefeat = true; break; }
            }
            if(hasDefeat) {
                for(int i=0; i<cell.objects.size(); ) {
                    if(HasProp(state, cell.objects[i].element, P_YOU)) {
                        cell.objects.erase(cell.objects.begin() + i);
                        changed = true;
                    } else i++;
                }
                if(changed) continue;
            }
            
            int sinkIdx = -1;
            int targetIdx = -1;
            
            for(int i=0; i<cell.objects.size(); i++) {
                if(HasProp(state, cell.objects[i].element, P_SINK)) {
                    // Find a target
                    for(int j=0; j<cell.objects.size(); j++) {
                        if(i == j) continue;
                        // Found a pair
                        sinkIdx = i;
                        targetIdx = j;
                        break;
                    }
                }
                if(sinkIdx != -1) break;
            }
            
            if(sinkIdx != -1 && targetIdx != -1) {
                // Remove larger index first to keep smaller index valid
                int first = (std::max)(sinkIdx, targetIdx);
                int second = (std::min)(sinkIdx, targetIdx);
                
                cell.objects.erase(cell.objects.begin() + first);
                cell.objects.erase(cell.objects.begin() + second);
                changed = true;
            }
        }
    }
}

// --- WIN CHECK ---
void CheckWin(GameState& state) {
    state.hasWon = false;
    for(const auto& cell : state.grid) {
        bool isYou = false, isWin = false;
        for(const auto& o : cell.objects) {
            if(HasProp(state, o.element, P_YOU)) isYou = true;
            if(HasProp(state, o.element, P_WIN)) isWin = true;
        }
        if(isYou && isWin) {
            state.hasWon = true;
            return;
        }
    }
}

GameState MakeMove(const GameState& state, int dx, int dy) {
    GameState newState = state;
    DoMove(newState, dx, dy, false);
    ParseRules(newState);
    ProcessInteractions(newState);
    ParseRules(newState); // Re-parse rules in case text was destroyed
    CheckWin(newState);
    return newState;
}

// --- OPTIMIZED SERIALIZATION ---
std::string SerializeState(GameState& s) {
    std::string res;
    res.reserve(currentWidth * currentHeight * 2);

    for(auto& cell : s.grid) {
        if (cell.objects.empty()) {
            res += (char)255; 
        } else {
            if (cell.objects.size() > 1) {
                std::sort(cell.objects.begin(), cell.objects.end(), 
                    [](const Object& a, const Object& b) { return a.element < b.element; });
            }
            for(auto& o : cell.objects) res += (char)o.element;
            res += (char)254; 
        }
    }
    return res;
}

// --- LOAD LEVELS ---
void LoadLevel(int idx, HWND hwnd) {
    if (idx >= levels.size()) idx = 0;
    currentLevelIndex = idx;
    
    const auto& mapData = levels[idx];
    currentHeight = mapData.size();
    currentWidth = currentHeight > 0 ? mapData[0].size() : 0;
    
    currentState.grid.clear();
    currentState.grid.resize(currentWidth * currentHeight);
    
    currentState.hasWon = false;
    undoStack.clear();

    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            int elem = CharToElement(mapData[y][x]);
            if (elem != EMPTY) {
                GetCell(currentState, x, y).objects.push_back({elem});
            }
        }
    }
    ParseRules(currentState);
    CheckWin(currentState);
    
    if (hwnd) {
        int newWidth = currentWidth * 40 + 20;
        int newHeight = currentHeight * 40 + 40;
        SetWindowPos(hwnd, NULL, 0, 0, newWidth, newHeight, SWP_NOMOVE | SWP_NOZORDER);
    }
}

void ResizeGrid(int newW, int newH) {
    if (newW < 1 || newH < 1) return;
    std::vector<Cell> newGrid(newW * newH);
    
    for (int y = 0; y < (std::min)(currentHeight, newH); y++) {
        for (int x = 0; x < (std::min)(currentWidth, newW); x++) {
            newGrid[y * newW + x] = currentState.grid[y * currentWidth + x];
        }
    }
    
    currentState.grid = newGrid;
    currentWidth = newW;
    currentHeight = newH;
    ParseRules(currentState);
}