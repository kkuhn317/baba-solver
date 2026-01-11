#include "solver.h"
#include "game.h"
#include "movement.h"
#include <queue>
#include <unordered_set>
#include <tuple>
#include <iostream>
#include <utility>
#include <algorithm> // For std::sort
#include <functional>

// --- AUTO SOLVER ---
std::string Solve(const GameState& startState) {
    std::queue<std::tuple<GameState, std::string, int>> q;
    std::unordered_set<std::string> visited;
    
    // We need a non-const copy to sort for the initial state
    GameState initial = startState;
    std::string initialHash = SerializeState(initial);
    
    q.push(std::make_tuple(initial, "", 0));
    visited.insert(initialHash);
    
    // Prioritize directions? usually R/D are more common openers, but standard is fine
    std::vector<std::tuple<int, int, char>> dirs = {
        {1, 0, 'R'}, {0, 1, 'D'}, {-1, 0, 'L'}, {0, -1, 'U'}
    };
    
    int currentDepth = 0;
    long long visitedCount = 1; 
    
    while(!q.empty()) {
        auto& front = q.front(); 
        GameState& state = std::get<0>(front);
        std::string path = std::get<1>(front);
        int depth = std::get<2>(front);
        
        // Check for win BEFORE generating children
        // (This saves 1 cycle of expansion)
        if(state.hasWon) return path;

        // Logging
        if(depth > currentDepth) {
            currentDepth = depth;
            std::cout << "Depth: " << currentDepth 
                      << " | Queue: " << q.size() 
                      << " | Unique States: " << visitedCount << std::endl;
        }

        // Generate Moves
        for(auto& dir : dirs) {
            int dx = std::get<0>(dir);
            int dy = std::get<1>(dir);
            char c = std::get<2>(dir);
            
            // Generate new state
            GameState newState = MakeMove(state, dx, dy);
            
            // Check cache
            // Note: MakeMove might have re-ordered objects, but SerializeState 
            // will enforce canonical sorting again.
            std::string ser = SerializeState(newState);
            
            if(visited.find(ser) == visited.end()) {
                visited.insert(ser);
                visitedCount++;
                q.push(std::make_tuple(std::move(newState), path + c, depth + 1));
            }
        }
        
        q.pop(); // Pop after processing to keep references valid
    }
    return "No solution found";
}

// --- OPTIMIZED SOLVER (PUSH ONLY) ---

// Helper: Find player position (assumes single YOU)
static std::pair<int, int> FindPlayerPos(GameState& state) {
    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            Cell& c = GetCell(state, x, y);
            for(auto& obj : c.objects) {
                if(HasProp(state, obj.element, P_YOU)) return {x, y};
            }
        }
    }
    return {-1, -1};
}

// Helper: BFS for pathfinding (walking only)
static std::string GetWalkPath(GameState& state, int sx, int sy, int ex, int ey) {
    if (sx == ex && sy == ey) return "";
    
    struct Node { int x, y; std::string path; };
    std::queue<Node> q;
    q.push({sx, sy, ""});
    
    std::vector<bool> visited(currentWidth * currentHeight, false);
    visited[sy * currentWidth + sx] = true;
    
    int dx[] = {1, -1, 0, 0};
    int dy[] = {0, 0, 1, -1};
    char dc[] = {'R', 'L', 'D', 'U'};
    
    while(!q.empty()) {
        Node curr = q.front(); q.pop();
        if (curr.x == ex && curr.y == ey) return curr.path;
        
        for(int i=0; i<4; i++) {
            int nx = curr.x + dx[i];
            int ny = curr.y + dy[i];
            
            if(nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight) {
                if(!visited[ny * currentWidth + nx]) {
                    bool blocked = false;
                    Cell& c = GetCell(state, nx, ny);
                    for(auto& obj : c.objects) {
                        if(HasProp(state, obj.element, P_STOP)) blocked = true;
                        if(HasProp(state, obj.element, P_PUSH)) blocked = true;
                    }
                    
                    if(!blocked) {
                        visited[ny * currentWidth + nx] = true;
                        q.push({nx, ny, curr.path + dc[i]});
                    }
                }
            }
        }
    }
    return "";
}

// Helper: Move player in state (teleport)
static void TeleportPlayer(GameState& state, int sx, int sy, int ex, int ey) {
    if (sx == ex && sy == ey) return;
    Cell& src = GetCell(state, sx, sy);
    Cell& dst = GetCell(state, ex, ey);
    
    for(auto it = src.objects.begin(); it != src.objects.end(); ) {
        if(HasProp(state, it->element, P_YOU)) {
            dst.objects.push_back(*it);
            it = src.objects.erase(it);
            break; 
        } else {
            ++it;
        }
    }
}

// Helper: Canonicalize state by moving player to top-left-most reachable position
static void CanonicalizeState(GameState& state) {
    std::pair<int, int> p = FindPlayerPos(state);
    int px = p.first;
    int py = p.second;
    if (px == -1) return;
    
    std::vector<bool> visited(currentWidth * currentHeight, false);
    std::queue<std::pair<int,int>> q;
    q.push({px, py});
    visited[py * currentWidth + px] = true;
    
    int minX = px, minY = py;
    int dx[] = {1, -1, 0, 0};
    int dy[] = {0, 0, 1, -1};
    
    while(!q.empty()) {
        std::pair<int, int> curr = q.front(); q.pop();
        int cx = curr.first;
        int cy = curr.second;
        
        if (cy < minY || (cy == minY && cx < minX)) {
            minX = cx; minY = cy;
        }
        
        for(int i=0; i<4; i++) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if(nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight) {
                if(!visited[ny * currentWidth + nx]) {
                    bool blocked = false;
                    Cell& c = GetCell(state, nx, ny);
                    for(auto& obj : c.objects) {
                        if(HasProp(state, obj.element, P_STOP)) blocked = true;
                        if(HasProp(state, obj.element, P_PUSH)) blocked = true;
                    }
                    if(!blocked) {
                        visited[ny * currentWidth + nx] = true;
                        q.push({nx, ny});
                    }
                }
            }
        }
    }
    TeleportPlayer(state, px, py, minX, minY);
}

struct StateNode {
    GameState state;
    std::string path;
    int pushes;
    
    bool operator>(const StateNode& other) const {
        if (pushes != other.pushes) return pushes > other.pushes;
        return path.length() > other.path.length();
    }
};

std::string SolveOptimized(const GameState& startState) {
    std::priority_queue<StateNode, std::vector<StateNode>, std::greater<StateNode>> pq;
    std::unordered_set<std::string> visited;
    
    // Start with the actual state. We will canonicalize on pop to check visited.
    pq.push({startState, "", 0});
    
    int dxs[] = {1, 0, -1, 0};
    int dys[] = {0, 1, 0, -1};
    char dcs[] = {'R', 'D', 'L', 'U'};
    
    int iterations = 0;

    while(!pq.empty()) {
        iterations++;
        if (iterations % 1000 == 0) {
            std::cout << "Optimized Solver: " << iterations << " states. PQ: " << pq.size() 
                      << " | Pushes: " << pq.top().pushes << std::endl;
        }

        StateNode current = pq.top(); pq.pop();
        GameState state = current.state;
        std::string path = current.path;
        int pushes = current.pushes;

        if (state.hasWon) return path;
        
        // Canonicalize for visited check
        GameState canon = state;
        CanonicalizeState(canon);
        std::string hash = SerializeState(canon);
        
        if (visited.find(hash) != visited.end()) continue;
        visited.insert(hash);

        std::pair<int, int> p = FindPlayerPos(state);
        int px = p.first;
        int py = p.second;
        if (px == -1) continue;
        
        // Re-run flood fill to find push points
        std::vector<bool> reach(currentWidth * currentHeight, false);
        std::queue<std::pair<int,int>> fq;
        fq.push({px, py});
        reach[py*currentWidth+px] = true;
        std::vector<std::pair<int,int>> reachable;
        
        while(!fq.empty()) {
            std::pair<int, int> curr = fq.front(); fq.pop();
            int cx = curr.first;
            int cy = curr.second;
            reachable.push_back({cx, cy});
            
            // Check for WIN at this reachable position
            Cell& cell = GetCell(state, cx, cy);
            for(auto& obj : cell.objects) {
                if(HasProp(state, obj.element, P_WIN)) {
                    std::string walk = GetWalkPath(state, px, py, cx, cy);
                    return path + walk;
                }
            }
            
            for(int i=0; i<4; i++) {
                int nx = cx + dxs[i], ny = cy + dys[i];
                if(nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight) {
                    if(!reach[ny*currentWidth+nx]) {
                        bool blocked = false;
                        Cell& c = GetCell(state, nx, ny);
                        for(auto& obj : c.objects) {
                            if(HasProp(state, obj.element, P_STOP)) blocked = true;
                            if(HasProp(state, obj.element, P_PUSH)) blocked = true;
                        }
                        if(!blocked) {
                            reach[ny*currentWidth+nx] = true;
                            fq.push({nx, ny});
                        }
                    }
                }
            }
        }
        
        for(auto& pos : reachable) {
            int rx = pos.first;
            int ry = pos.second;
            for(int i=0; i<4; i++) {
                int dx = dxs[i], dy = dys[i];
                int tx = rx + dx, ty = ry + dy;
                if(tx < 0 || tx >= currentWidth || ty < 0 || ty >= currentHeight) continue;
                
                bool isPush = false;
                Cell& target = GetCell(state, tx, ty);
                for(auto& obj : target.objects) {
                    if(HasProp(state, obj.element, P_PUSH)) { isPush = true; break; }
                }
                
                if(isPush && CanMove(state, rx, ry, dx, dy)) {
                    GameState nextState = state;
                    TeleportPlayer(nextState, px, py, rx, ry);
                    nextState = MakeMove(nextState, dx, dy);
                    
                    std::string walk = GetWalkPath(state, px, py, rx, ry);
                    pq.push({nextState, path + walk + dcs[i], pushes + 1});
                }
            }
        }
    }
    return "No solution found";
}