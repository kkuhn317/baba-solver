#ifndef CONSTANTS_H
#define CONSTANTS_H

// --- CONSTANTS ---
const int TILE_SIZE = 40;
extern int GRID_WIDTH;
extern int GRID_HEIGHT;

// --- ENTITY DEFINITIONS ---
enum Element {
    EMPTY = 0, WALL = 1, BABA = 2, ROCK = 3, FLAG = 4, WATER = 5,
    // Text IDs start at 10 to separate them from objects
    TEXT_BABA = 10, TEXT_ROCK = 11, TEXT_FLAG = 12, TEXT_WALL = 13, TEXT_WATER = 14,
    TEXT_IS = 20, TEXT_YOU = 21, TEXT_PUSH = 22, TEXT_WIN = 23, TEXT_STOP = 24, TEXT_SINK = 25
};

// Properties bitmask
enum PropFlags { P_NONE = 0, P_YOU = 1, P_PUSH = 2, P_STOP = 4, P_WIN = 8, P_SINK = 16 };

// --- COLORS ---
const COLORREF C_BG = RGB(42, 44, 48);
const COLORREF C_WALL = RGB(111, 118, 131);
const COLORREF C_BABA = RGB(255, 255, 255);
const COLORREF C_ROCK = RGB(168, 111, 80);
const COLORREF C_FLAG = RGB(247, 216, 97);
const COLORREF C_TEXT_PINK = RGB(255, 68, 153);
const COLORREF C_TEXT_WHITE = RGB(255, 255, 255);
const COLORREF C_WATER = RGB(0, 119, 211);

#endif