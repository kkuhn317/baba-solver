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

void DrawRect(HDC hdc, int x, int y, COLORREF color, const char* text = nullptr) {
    HBRUSH brush = CreateSolidBrush(color);
    RECT rect = { x * TILE_SIZE, y * TILE_SIZE, (x + 1) * TILE_SIZE, (y + 1) * TILE_SIZE };
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
    if (text) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));
        TextOutA(hdc, x * TILE_SIZE + 8, y * TILE_SIZE + 10, text, (int)strlen(text));
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
                    COLORREF c = RGB(255,0,255); const char* t = "";
                    switch(obj.element) {
                        case WALL: c = C_WALL; break;
                        case BABA: c = C_BABA; break;
                        case FLAG: c = C_FLAG; break;
                        case ROCK: c = C_ROCK; break;
                        case TEXT_BABA: c = C_TEXT_PINK; t="BABA"; break;
                        case TEXT_FLAG: c = C_TEXT_PINK; t="FLAG"; break;
                        case TEXT_WALL: c = C_TEXT_PINK; t="WALL"; break;
                        case TEXT_ROCK: c = C_TEXT_PINK; t="ROCK"; break;
                        case TEXT_IS:   c = C_TEXT_WHITE; t="IS"; break;
                        case TEXT_YOU:  c = C_TEXT_PINK; t="YOU"; break;
                        case TEXT_WIN:  c = C_TEXT_PINK; t="WIN"; break;
                        case TEXT_STOP: c = C_TEXT_PINK; t="STOP"; break;
                        case TEXT_PUSH: c = C_TEXT_PINK; t="PUSH"; break;
                    }
                    DrawRect(hdc, x, y, c, t);
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

        if(dx!=0 || dy!=0) {
            if (currentState.hasWon) {
                // If previously won, pressing a key loads next level
                LoadLevel(currentLevelIndex + 1, hwnd);
            } else {
                DoMove(currentState, dx, dy);
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