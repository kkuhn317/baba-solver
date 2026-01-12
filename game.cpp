#include "game.h"
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

// --- MAPPING CHARS TO OBJECTS ---
int CharToElement(char c) {
    switch(c) {
        case '#': return WALL;
        case 'B': return BABA;
        case 'F': return FLAG;
        case 'R': return ROCK;
        case 'W': return WATER;
        case 'b': return TEXT_BABA;
        case 'f': return TEXT_FLAG;
        case 'w': return TEXT_WALL;
        case 'r': return TEXT_ROCK;
        case 'a': return TEXT_WATER;
        case 'i': return TEXT_IS;
        case 'y': return TEXT_YOU;
        case 'n': return TEXT_WIN;
        case 's': return TEXT_STOP;
        case 'p': return TEXT_PUSH;
        case 'k': return TEXT_SINK;
        default: return EMPTY;
    }
}

// --- LOGIC HELPERS ---
bool IsNoun(int e) { return (e >= TEXT_BABA && e <= TEXT_WATER); }
bool IsProperty(int e) { return (e >= TEXT_YOU && e <= TEXT_SINK); }
int TextToElement(int textID) {
    if (textID == TEXT_BABA) return BABA;
    if (textID == TEXT_ROCK) return ROCK;
    if (textID == TEXT_FLAG) return FLAG;
    if (textID == TEXT_WALL) return WALL;
    if (textID == TEXT_WATER) return WATER;
    return 0;
}
PropFlags TextToProp(int textID) {
    if (textID == TEXT_YOU) return P_YOU;
    if (textID == TEXT_PUSH) return P_PUSH;
    if (textID == TEXT_STOP) return P_STOP;
    if (textID == TEXT_WIN) return P_WIN;
    if (textID == TEXT_SINK) return P_SINK;
    return P_NONE;
}

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