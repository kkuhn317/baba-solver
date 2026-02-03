#include <windows.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <set>
#include <queue>
#include <unordered_set>
#include <tuple>
#include <utility>
#include <iostream>
#include <fstream>
#include "constants.h"
#include "definitions.h"
#include "game.h"
#include "movement.h"
#include "solver.h"
#include "levels.h"

HWND hPaletteWnd = NULL;

void DrawRect(HDC hdc, int x, int y, COLORREF bgColor, const char* text = nullptr, COLORREF textColor = RGB(0,0,0), bool transparent = false) {
    if (!transparent) {
        HBRUSH brush = CreateSolidBrush(bgColor);
        RECT rect = { x * TILE_SIZE, y * TILE_SIZE, (x + 1) * TILE_SIZE, (y + 1) * TILE_SIZE };
        FillRect(hdc, &rect, brush);
        DeleteObject(brush);
    }
    if (text) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);
        
        size_t len = strlen(text);
        if (len >= 4) {
            std::string s(text);
            std::string l1 = s.substr(0, 2);
            std::string l2 = s.substr(2);
            TextOutA(hdc, x * TILE_SIZE + 8, y * TILE_SIZE + 2, l1.c_str(), (int)l1.length());
            TextOutA(hdc, x * TILE_SIZE + 8, y * TILE_SIZE + 18, l2.c_str(), (int)l2.length());
        } else {
            TextOutA(hdc, x * TILE_SIZE + 8, y * TILE_SIZE + 10, text, (int)len);
        }
    }
}

void GetDrawParams(int element, COLORREF& bg, COLORREF& txt, const char*& t, bool& transparent) {
    bg = RGB(0,0,0); 
    txt = RGB(0,0,0);
    t = nullptr;
    transparent = false;

    switch(element) {
        case WALL: bg = C_WALL; break;
        case BABA: bg = C_BABA; break;
        case FLAG: bg = C_FLAG; break;
        case ROCK: bg = C_ROCK; break;
        case WATER: bg = C_WATER; break;
        case SKULL: bg = C_DEFEAT; break;
        case TEXT_BABA: t="BABA"; txt = C_TEXT_PINK; transparent = true; break;
        case TEXT_FLAG: t="FLAG"; txt = C_FLAG; transparent = true; break;
        case TEXT_WALL: t="WALL"; txt = C_WALL; transparent = true; break;
        case TEXT_ROCK: t="ROCK"; txt = C_ROCK; transparent = true; break;
        case TEXT_WATER: t="WATER"; txt = C_WATER; transparent = true; break;
        case TEXT_SKULL: t="SKULL"; txt = C_DEFEAT; transparent = true; break;
        case TEXT_IS:   t="IS";   txt = C_TEXT_WHITE; transparent = true; break;
        case TEXT_YOU:  t="YOU";  bg = C_TEXT_PINK; break;
        case TEXT_WIN:  t="WIN";  bg = C_FLAG; break;
        case TEXT_STOP: t="STOP"; bg = RGB(0, 200, 0); break;
        case TEXT_PUSH: t="PUSH"; bg = C_ROCK; break;
        case TEXT_SINK: t="SINK"; bg = C_WATER; break;
        case TEXT_DEFEAT: t="DEFEAT"; bg = C_DEFEAT; break;
    }
}

LRESULT CALLBACK PaletteProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);

        int cols = 4;
        for (size_t i = 0; i < editorPalette.size(); i++) {
            int x = (int)i % cols;
            int y = (int)i / cols;
            
            COLORREF bg, txt;
            const char* t;
            bool transp;
            GetDrawParams(editorPalette[i], bg, txt, t, transp);
            DrawRect(hdc, x, y, bg, t, txt, transp);

            if ((int)i == editorPaletteIdx) {
                HPEN pen = CreatePen(PS_SOLID, 3, RGB(255, 255, 0));
                HGDIOBJ old = SelectObject(hdc, pen);
                SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, x * TILE_SIZE, y * TILE_SIZE, (x + 1) * TILE_SIZE, (y + 1) * TILE_SIZE);
                SelectObject(hdc, old);
                DeleteObject(pen);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam) / TILE_SIZE;
        int y = HIWORD(lParam) / TILE_SIZE;
        int cols = 4;
        int idx = y * cols + x;
        if (idx >= 0 && idx < (int)editorPalette.size()) {
            editorPaletteIdx = idx;
            InvalidateRect(hwnd, NULL, TRUE);
            HWND hMain = GetWindow(hwnd, GW_OWNER);
            if(hMain) InvalidateRect(hMain, NULL, TRUE);
        }
        return 0;
    }
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void ExportLevel() {
    std::cout << "\n--- EXPORTED LEVEL DATA ---" << std::endl;
    
    // 1. Build Legend
    std::map<char, std::string> legend;
    std::map<int, char> elemToChar;
    
    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            Cell& c = GetCell(currentState, x, y);
            if(!c.objects.empty()) {
                int e = c.objects.back().element;
                if(elemToChar.find(e) == elemToChar.end()) {
                    char ch = GetDefaultChar(e);
                    elemToChar[e] = ch;
                    legend[ch] = GetElementName(e);
                }
            }
        }
    }
    
    std::cout << "{ {";
    for(auto& kv : legend) std::cout << "{'" << kv.first << "',\"" << kv.second << "\"}, ";
    std::cout << "},\n{" << std::endl;
    
    for(int y=0; y<currentHeight; y++) {
        std::cout << "    \"";
        for(int x=0; x<currentWidth; x++) {
            Cell& c = GetCell(currentState, x, y);
            if(c.objects.empty()) std::cout << ".";
            else std::cout << elemToChar[c.objects.back().element];
        }
        std::cout << "\"," << std::endl;
    }
    std::cout << "} }" << std::endl;
}

void SaveLevelToFile(const char* filename) {
    std::ofstream out(filename);
    if (!out) return;
    
    // 1. Build Legend
    std::map<char, std::string> legend;
    std::map<int, char> elemToChar;
    
    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            Cell& c = GetCell(currentState, x, y);
            if(!c.objects.empty()) {
                int e = c.objects.back().element;
                if(elemToChar.find(e) == elemToChar.end()) {
                    char ch = GetDefaultChar(e);
                    elemToChar[e] = ch;
                    legend[ch] = GetElementName(e);
                }
            }
        }
    }
    
    out << "[LEGEND]\n";
    for(auto& kv : legend) out << kv.first << "=" << kv.second << "\n";
    
    out << "[GRID]\n";
    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            Cell& c = GetCell(currentState, x, y);
            if(c.objects.empty()) out << ".";
            else out << elemToChar[c.objects.back().element];
        }
        out << "\n";
    }
    out.close();
}

void LoadLevelFromFile(const char* filename, HWND hwnd) {
    std::ifstream in(filename);
    if (!in) return;
    
    std::vector<std::string> gridLines;
    std::map<char, int> charToElem;
    std::string line;
    bool readingGrid = false;
    
    while(std::getline(in, line)) {
        if(line.empty()) continue;
        if(line == "[GRID]") { readingGrid = true; continue; }
        if(line == "[LEGEND]") { readingGrid = false; continue; }
        
        if(!readingGrid) {
            size_t eq = line.find('=');
            if(eq != std::string::npos) {
                char key = line[0];
                std::string name = line.substr(eq+1);
                charToElem[key] = ElementFromString(name);
            }
        } else {
            gridLines.push_back(line);
        }
    }
    in.close();
    if(gridLines.empty()) return;

    currentHeight = (int)gridLines.size();
    currentWidth = (int)gridLines[0].size();
    currentState.grid.clear();
    currentState.grid.resize(currentWidth * currentHeight);
    currentState.hasWon = false;
    undoStack.clear();

    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            if (x < (int)gridLines[y].size()) {
                char c = gridLines[y][x];
                int elem = charToElem.count(c) ? charToElem[c] : EMPTY;
                if (elem != EMPTY) GetCell(currentState, x, y).objects.push_back({elem});
            }
        }
    }
    ParseRules(currentState);
    CheckWin(currentState);
    
    int newWidth = currentWidth * TILE_SIZE + 20;
    int newHeight = currentHeight * TILE_SIZE + 40;
    SetWindowPos(hwnd, NULL, 0, 0, newWidth, newHeight, SWP_NOMOVE | SWP_NOZORDER);
}

// --- WINDOWS MAIN ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        // Draw BG
        HBRUSH bg = CreateSolidBrush(C_BG);
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        // Draw Grid
        for (int y = 0; y < currentHeight; y++) {
            for (int x = 0; x < currentWidth; x++) {
                if (GetCell(currentState, x, y).objects.empty()) continue;
                for (auto& obj : GetCell(currentState, x, y).objects) {
                    COLORREF bg = RGB(0,0,0); 
                    COLORREF txt = RGB(0,0,0);
                    const char* t = nullptr;
                    bool transparent = false;
                    GetDrawParams(obj.element, bg, txt, t, transparent);
                    DrawRect(hdc, x, y, bg, t, txt, transparent);
                }
            }
        }
        
        if (currentState.hasWon) {
             SetBkMode(hdc, TRANSPARENT);
             SetTextColor(hdc, RGB(0, 255, 0));
             TextOutA(hdc, 10, 10, "WIN! NEXT LEVEL...", 18);
        }
        
        if (isEditorMode) {
            // Draw Editor UI Overlay
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, RGB(50, 50, 50));
            SetTextColor(hdc, RGB(255, 255, 255));
            TextOutA(hdc, 10, 10, " EDITOR MODE ", 13);
            
            char buf[64];
            int elem = editorPalette[editorPaletteIdx];
            std::string name = GetElementName(elem);
            sprintf_s(buf, "Selected: %s", name.c_str());
            TextOutA(hdc, 10, 30, buf, (int)strlen(buf));
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        if (isEditorMode) {
            int x = LOWORD(lParam) / TILE_SIZE;
            int y = HIWORD(lParam) / TILE_SIZE;
            if (x >= 0 && x < currentWidth && y >= 0 && y < currentHeight) {
                Cell& c = GetCell(currentState, x, y);
                if (uMsg == WM_LBUTTONDOWN) {
                    c.objects.push_back({editorPalette[editorPaletteIdx]});
                } else {
                    if (!c.objects.empty()) c.objects.pop_back();
                }
                ParseRules(currentState); // Update rules immediately
                InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        return 0;
    }
    case WM_KEYDOWN: {
        int dx=0, dy=0;
        if(wParam == VK_LEFT) dx=-1;
        if(wParam == VK_RIGHT) dx=1;
        if(wParam == VK_UP) dy=-1;
        if(wParam == VK_DOWN) dy=1;
        if(wParam == 'R') { LoadLevel(currentLevelIndex, hwnd); InvalidateRect(hwnd, NULL, TRUE); return 0; }
        if(wParam >= '1' && wParam <= '9') {
            int level = wParam - '1';
            if (level < levels.size()) {
                LoadLevel(level, hwnd);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }
        if(wParam == 'Z') {
            if (!undoStack.empty()) {
                currentState = undoStack.back();
                undoStack.pop_back();
                ParseRules(currentState);
                CheckWin(currentState);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }
        
        // Editor Toggles
        if(wParam == 'E') {
            isEditorMode = !isEditorMode;
            SetWindowTextA(hwnd, isEditorMode ? "Native Baba - EDITOR MODE" : "Native Baba");
            ShowWindow(hPaletteWnd, isEditorMode ? SW_SHOW : SW_HIDE);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if(isEditorMode) {
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                bool resized = false;
                if (wParam == VK_RIGHT) { ResizeGrid(currentWidth + 1, currentHeight); resized = true; }
                else if (wParam == VK_LEFT) { ResizeGrid(currentWidth - 1, currentHeight); resized = true; }
                else if (wParam == VK_DOWN) { ResizeGrid(currentWidth, currentHeight + 1); resized = true; }
                else if (wParam == VK_UP) { ResizeGrid(currentWidth, currentHeight - 1); resized = true; }

                if (resized) {
                    int newWidth = currentWidth * TILE_SIZE + 20;
                    int newHeight = currentHeight * TILE_SIZE + 40;
                    SetWindowPos(hwnd, NULL, 0, 0, newWidth, newHeight, SWP_NOMOVE | SWP_NOZORDER);
                    InvalidateRect(hwnd, NULL, TRUE);
                    return 0;
                }
            }

            if(wParam == 'W') {
                editorPaletteIdx = (editorPaletteIdx + 1) % editorPalette.size();
                InvalidateRect(hwnd, NULL, TRUE);
                if(hPaletteWnd) InvalidateRect(hPaletteWnd, NULL, TRUE);
            }
            if(wParam == 'Q') {
                editorPaletteIdx = (editorPaletteIdx - 1 + editorPalette.size()) % editorPalette.size();
                InvalidateRect(hwnd, NULL, TRUE);
                if(hPaletteWnd) InvalidateRect(hPaletteWnd, NULL, TRUE);
            }
            if(wParam == 'S') {
                SaveLevelToFile("level.txt");
                MessageBoxA(hwnd, "Level saved to level.txt", "Editor", MB_OK);
            }
            if(wParam == 'L') {
                LoadLevelFromFile("level.txt", hwnd);
                MessageBoxA(hwnd, "Level loaded from level.txt", "Editor", MB_OK);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            if(wParam == 'X') {
                ExportLevel();
                MessageBoxA(NULL, "Level exported to console.", "Editor", MB_OK);
            }
        } else {
            if(wParam == 'S') {
                std::string sol = Solve(currentState);
                MessageBoxA(NULL, sol.c_str(), "Solution", MB_OK);
                return 0;
            }
            if(wParam == 'P') {
                std::string sol = SolveOptimized(currentState);
                MessageBoxA(NULL, sol.c_str(), "Optimized Solution", MB_OK);
                return 0;
            }
            if(wParam == 'L') {
                std::string sol = SolveLogic(currentState);
                MessageBoxA(NULL, sol.c_str(), "Logic Solution", MB_OK);
                return 0;
            }
            
            if(dx!=0 || dy!=0) {
            if (currentState.hasWon) {
                // If previously won, pressing a key loads next level
                LoadLevel(currentLevelIndex + 1, hwnd);
            } else {
                DoMove(currentState, dx, dy);
                ParseRules(currentState);
                ProcessInteractions(currentState);
                ParseRules(currentState);
                CheckWin(currentState);
            }
            // Update Title
            char buf[64];
            sprintf_s(buf, "Native Baba - Level %d", currentLevelIndex + 1);
            SetWindowTextA(hwnd, buf);
            InvalidateRect(hwnd, NULL, TRUE);
            }
        }
        return 0;
    }
    case WM_DESTROY: DestroyWindow(hPaletteWnd); PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR args, int nShow) {
    // 1. Create the console window
    AllocConsole();

    // 2. Connect std::cout and std::cerr to the new console window
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);

    std::cout << "--- BABA SOLVER CONTROLS ---" << std::endl;
    std::cout << "ARROWS : Move" << std::endl;
    std::cout << "R      : Reset Level" << std::endl;
    std::cout << "Z      : Undo Move" << std::endl;
    std::cout << "1-9    : Load Level" << std::endl;
    std::cout << "----------------------------" << std::endl;
    std::cout << "S      : Run Basic Solver (BFS)" << std::endl;
    std::cout << "P      : Run Push-Optimized Solver" << std::endl;
    std::cout << "L      : Run Logic Solver" << std::endl;
    std::cout << "----------------------------" << std::endl;
    std::cout << "E      : Toggle Editor Mode" << std::endl;
    std::cout << "Q/W    : Cycle Tile (or use Palette)" << std::endl;
    std::cout << "L-Click: Place Tile" << std::endl;
    std::cout << "R-Click: Remove Tile" << std::endl;
    std::cout << "S/L    : Save/Load Level (in Editor)" << std::endl;
    std::cout << "Ctrl+Arr : Resize Level (in Editor)" << std::endl;
    std::cout << "----------------------------" << std::endl;

    WNDCLASS wc = { 0, WindowProc, 0, 0, hInst, 0, LoadCursor(0, IDC_ARROW), 0, 0, "BabaClass" };
    RegisterClass(&wc);
    int initialWidth = 20 * TILE_SIZE + 20;
    int initialHeight = 12 * TILE_SIZE + 40;
    HWND hwnd = CreateWindowEx(0, "BabaClass", "Native Baba", WS_OVERLAPPEDWINDOW, 100, 100, 
        initialWidth, initialHeight, 0, 0, hInst, 0);
    
    // Create Palette Window
    WNDCLASS wcPal = { 0, PaletteProc, 0, 0, hInst, 0, LoadCursor(0, IDC_ARROW), 0, 0, "BabaPalette" };
    RegisterClass(&wcPal);
    
    hPaletteWnd = CreateWindowEx(WS_EX_TOOLWINDOW, "BabaPalette", "Palette", WS_CAPTION | WS_SYSMENU, 
        100 + initialWidth + 10, 100, 180, 240, hwnd, 0, hInst, 0);

    LoadLevel(0, hwnd);
    ShowWindow(hwnd, nShow);
    MSG msg;
    while(GetMessage(&msg, 0, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}