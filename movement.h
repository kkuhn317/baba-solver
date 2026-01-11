#ifndef MOVEMENT_H
#define MOVEMENT_H

// Forward declaration
struct GameState;

// --- FUNCTIONS ---
bool CanMove(GameState& state, int x, int y, int dx, int dy);
void DoPush(GameState& state, int x, int y, int dx, int dy);
void DoMove(GameState& state, int dx, int dy, bool recordUndo = true);

#endif