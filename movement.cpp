#include "movement.h"
#include "game.h"
#include <algorithm>
#include <vector>

// --- MOVEMENT ---
bool CanMove(GameState& state, int x, int y, int dx, int dy) {
    int nx = x + dx, ny = y + dy;
    if (nx < 0 || nx >= currentWidth || ny < 0 || ny >= currentHeight) return false;
    
    Cell& target = GetCell(state, nx, ny);
    
    for (auto& obj : target.objects) {
        if (HasProp(state, obj.element, P_STOP) && !HasProp(state, obj.element, P_PUSH)) return false;
        if (HasProp(state, obj.element, P_PUSH)) {
            if (!CanMove(state, nx, ny, dx, dy)) return false;
        }
    }
    return true;
}

void DoPush(GameState& state, int x, int y, int dx, int dy) {
    int nx = x + dx, ny = y + dy;
    if (nx < 0 || nx >= currentWidth || ny < 0 || ny >= currentHeight) return;

    Cell& curr = GetCell(state, nx, ny);
    for (int i = 0; i < curr.objects.size(); i++) {
        if (HasProp(state, curr.objects[i].element, P_PUSH)) {
            DoPush(state, nx, ny, dx, dy);
            
            Cell& src = GetCell(state, nx, ny);
            Cell& dst = GetCell(state, nx+dx, ny+dy);
            
            dst.objects.push_back(src.objects[i]);
            src.objects.erase(src.objects.begin() + i);
            i--;
        }
    }
}

void DoMove(GameState& state, int dx, int dy, bool recordUndo) {
    if (recordUndo) undoStack.push_back(state);
    
    struct M { int x, y, idx; };
    std::vector<M> moves;
    
    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            Cell& c = GetCell(state, x, y);
            for(int i=0; i<c.objects.size(); i++) {
                if(HasProp(state, c.objects[i].element, P_YOU)) {
                    moves.push_back({x, y, i});
                }
            }
        }
    }

    if (dx > 0) std::sort(moves.begin(), moves.end(), [](const M& a, const M& b){ return a.x > b.x; });
    else if (dx < 0) std::sort(moves.begin(), moves.end(), [](const M& a, const M& b){ return a.x < b.x; });
    else if (dy > 0) std::sort(moves.begin(), moves.end(), [](const M& a, const M& b){ return a.y > b.y; });
    else std::sort(moves.begin(), moves.end(), [](const M& a, const M& b){ return a.y < b.y; });

    for(auto& m : moves) {
        if (CanMove(state, m.x, m.y, dx, dy)) {
            DoPush(state, m.x, m.y, dx, dy);
            
            Cell& src = GetCell(state, m.x, m.y);
            if (!src.objects.empty()) {
                for(int i=0; i<src.objects.size(); i++) {
                     if(HasProp(state, src.objects[i].element, P_YOU)) {
                         Cell& dst = GetCell(state, m.x+dx, m.y+dy);
                         dst.objects.push_back(src.objects[i]);
                         src.objects.erase(src.objects.begin() + i);
                         break;
                     }
                }
            }
        }
    }
}