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
#include "constants.h"
#include "game.h"
#include "movement.h"
#include "solver.h"
#include "levels.h"

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

                    switch(obj.element) {
                        case WALL: bg = C_WALL; break;
                        case BABA: bg = C_BABA; break;
                        case FLAG: bg = C_FLAG; break;
                        case ROCK: bg = C_ROCK; break;
                        case WATER: bg = C_WATER; break;
                        case TEXT_BABA: t="BABA"; txt = C_TEXT_PINK; transparent = true; break;
                        case TEXT_FLAG: t="FLAG"; txt = C_FLAG; transparent = true; break;
                        case TEXT_WALL: t="WALL"; txt = C_WALL; transparent = true; break;
                        case TEXT_ROCK: t="ROCK"; txt = C_ROCK; transparent = true; break;
                        case TEXT_WATER: t="WATER"; txt = C_WATER; transparent = true; break;
                        case TEXT_IS:   t="IS";   txt = C_TEXT_WHITE; transparent = true; break;
                        case TEXT_YOU:  t="YOU";  bg = C_TEXT_PINK; break;
                        case TEXT_WIN:  t="WIN";  bg = C_FLAG; break;
                        case TEXT_STOP: t="STOP"; bg = RGB(0, 200, 0); break;
                        case TEXT_PUSH: t="PUSH"; bg = C_ROCK; break;
                        case TEXT_SINK: t="SINK"; bg = C_WATER; break;
                    }
                    DrawRect(hdc, x, y, bg, t, txt, transparent);
                }
            }
        }
        
        if (currentState.hasWon) {
             SetBkMode(hdc, TRANSPARENT);
             SetTextColor(hdc, RGB(0, 255, 0));
             TextOutA(hdc, 10, 10, "WIN! NEXT LEVEL...", 18);
        }

        EndPaint(hwnd, &ps);
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
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
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

    WNDCLASS wc = { 0, WindowProc, 0, 0, hInst, 0, LoadCursor(0, IDC_ARROW), 0, 0, "BabaClass" };
    RegisterClass(&wc);
    int initialWidth = 20 * TILE_SIZE + 20;
    int initialHeight = 12 * TILE_SIZE + 40;
    HWND hwnd = CreateWindowEx(0, "BabaClass", "Native Baba", WS_OVERLAPPEDWINDOW, 100, 100, 
        initialWidth, initialHeight, 0, 0, hInst, 0);
    
    LoadLevel(0, hwnd);
    ShowWindow(hwnd, nShow);
    MSG msg;
    while(GetMessage(&msg, 0, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}