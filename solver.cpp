#include "solver.h"
#include "definitions.h"
#include "game.h"
#include <queue>
#include <unordered_set>
#include <tuple>
#include <iostream>
#include <utility>
#include <algorithm>
#include <functional>
#include <set>
#include <map>
#include <cmath> // Required for abs()

// External declaration from movement.cpp
void DoPush(GameState& state, int x, int y, int dx, int dy);

// --- HELPERS ---

// Checks if a cell is safe to walk on (not blocked, not a hazard)
static bool IsWalkable(const GameState& state, int x, int y, bool isMelt) {
    GameState& s = const_cast<GameState&>(state);
    const Cell& c = GetCell(s, x, y);
    for(const auto& obj : c.objects) {
        if(HasProp(s, obj.element, P_STOP) || 
           HasProp(s, obj.element, P_PUSH) || 
           HasProp(s, obj.element, P_SINK) || 
           HasProp(s, obj.element, P_DEFEAT)) {
            return false;
        }
        if (isMelt && HasProp(s, obj.element, P_HOT)) {
            return false;
        }
    }
    return true;
}

// Returns a boolean mask of all cells reachable by walking from (sx, sy)
static std::vector<bool> GetReachableCells(const GameState& state, const std::vector<std::pair<int, int>>& starts) {
    std::vector<bool> visited(currentWidth * currentHeight, false);
    std::queue<std::pair<int,int>> q;

    bool isMelt = false;
    for(int i=0; i<100; i++) {
        if ((state.propertyMap[i] & P_YOU) && (state.propertyMap[i] & P_MELT)) {
            isMelt = true;
            break;
        }
    }

    for(const auto& p : starts) {
        if (p.first >= 0 && p.second >= 0 && !visited[p.second * currentWidth + p.first]) {
            visited[p.second * currentWidth + p.first] = true;
            q.push(p);
        }
    }
    if (q.empty()) return visited;
    
    int dx[] = {1, -1, 0, 0};
    int dy[] = {0, 0, 1, -1};
    
    while(!q.empty()) {
        auto [cx, cy] = q.front(); q.pop();
        
        for(int i=0; i<4; i++) {
            int nx = cx + dx[i], ny = cy + dy[i];
            if(nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight) {
                if(!visited[ny * currentWidth + nx] && IsWalkable(state, nx, ny, isMelt)) {
                    visited[ny * currentWidth + nx] = true;
                    q.push({nx, ny});
                }
            }
        }
    }
    return visited;
}

// Returns a boolean mask of cells that are reachable OR adjacent to reachable cells
static std::vector<bool> GetAccessibleCells(const std::vector<bool>& reachable) {
    std::vector<bool> accessible = reachable;
    int dx[] = {1, -1, 0, 0};
    int dy[] = {0, 0, 1, -1};

    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            if (reachable[y*currentWidth+x]) {
                for(int i=0; i<4; i++) {
                    int nx = x + dx[i], ny = y + dy[i];
                    if(nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight) {
                        accessible[ny*currentWidth+nx] = true;
                    }
                }
            }
        }
    }
    return accessible;
}

// --- BASIC SOLVER (BFS/DFS) ---
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
        q.pop(); 
    }
    return "No solution found";
}

// --- PUSH OPTIMIZED SOLVER ---

// Helper: Find player position (assumes single YOU)
std::pair<int, int> FindPlayerPos(const GameState& state) {
    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            const Cell& c = GetCell(const_cast<GameState&>(state), x, y);
            for(const auto& obj : c.objects) {
                if(HasProp(const_cast<GameState&>(state), obj.element, P_YOU)) return {x, y};
            }
        }
    }
    return {-1, -1};
}

// Helper: Find all player positions
std::vector<std::pair<int, int>> FindAllPlayerPos(const GameState& state) {
    std::vector<std::pair<int, int>> players;
    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            const Cell& c = GetCell(const_cast<GameState&>(state), x, y);
            for(const auto& obj : c.objects) {
                if(HasProp(const_cast<GameState&>(state), obj.element, P_YOU)) {
                    players.push_back({x, y});
                    break; 
                }
            }
        }
    }
    return players;
}

// Helper: BFS for pathfinding (walking only)
static std::string GetWalkPath(const GameState& state, int sx, int sy, int ex, int ey) {
    if (sx == ex && sy == ey) return "";

    bool isMelt = false;
    for(int i=0; i<100; i++) {
        if ((state.propertyMap[i] & P_YOU) && (state.propertyMap[i] & P_MELT)) {
            isMelt = true;
            break;
        }
    }

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
            int nx = curr.x + dx[i], ny = curr.y + dy[i];
            if(nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight) {
                if(!visited[ny * currentWidth + nx]) {
                    if(IsWalkable(state, nx, ny, isMelt)) {
                        visited[ny * currentWidth + nx] = true;
                        q.push({nx, ny, curr.path + dc[i]});
                    }
                }
            }
        }
    }
    return "";
}

static void TeleportPlayer(GameState& state, int sx, int sy, int ex, int ey) {
    if (sx == ex && sy == ey) return;
    Cell& src = GetCell(state, sx, sy);
    Cell& dst = GetCell(state, ex, ey);
    for(auto it = src.objects.begin(); it != src.objects.end(); ) {
        if(HasProp(state, it->element, P_YOU)) {
            dst.objects.push_back(*it);
            it = src.objects.erase(it);
            break; 
        } else { ++it; }
    }
}

static void CanonicalizeState(GameState& state) {
    std::pair<int, int> p = FindPlayerPos(state);
    int px = p.first; int py = p.second;
    if (px == -1) return;
    
    auto reachable = GetReachableCells(state, {{px, py}});
    
    // Find top-left most reachable cell (Row-major scan finds min Y then min X)
    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            if(reachable[y*currentWidth+x]) {
                TeleportPlayer(state, px, py, x, y);
                return;
            }
        }
    }
}

int GetHeuristic(const GameState& state, int targetNoun, int targetProp) {
    std::vector<std::pair<int,int>> players;
    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            Cell& c = GetCell(const_cast<GameState&>(state), x, y);
            for(auto& obj : c.objects) {
                if(HasProp(const_cast<GameState&>(state), obj.element, P_YOU)) {
                    players.push_back({x, y});
                }
            }
        }
    }
    if (players.empty()) return 1000;

    int minDist = 1000;

    if (targetNoun != -1) {
        return 0;
    } else {
        for(int y=0; y<currentHeight; y++) {
            for(int x=0; x<currentWidth; x++) {
                Cell& c = GetCell(const_cast<GameState&>(state), x, y);
                for(auto& obj : c.objects) {
                    if(HasProp(const_cast<GameState&>(state), obj.element, P_WIN)) {
                        for(auto& p : players) {
                            int d = std::abs(p.first - x) + std::abs(p.second - y);
                            if (d < minDist) minDist = d;
                        }
                    }
                }
            }
        }
    }
    return (minDist == 1000) ? 0 : minDist;
}

struct StateNode {
    GameState state;
    std::string path;
    int pushes;
    int heuristic; // A* Cost

    // Priority Queue sorts primarily by Pushes (Cost), using Heuristic as tie-breaker
    bool operator>(const StateNode& other) const {
        // Prioritize Pushes FIRST, then Heuristic (Distance) as tie-breaker.
        if (pushes != other.pushes) return pushes > other.pushes;
        return heuristic > other.heuristic;
    }
};

std::string SolveOptimized(const GameState& startState, int targetNoun, int targetProp, int maxIterations) {
    std::priority_queue<StateNode, std::vector<StateNode>, std::greater<StateNode>> pq;
    std::unordered_set<std::string> visited;
    
    // 1. Calculate Initial Heuristic
    int h = GetHeuristic(startState, targetNoun, targetProp);
    pq.push({startState, "", 0, h});
    
    int dxs[] = {1, 0, -1, 0};
    int dys[] = {0, 1, 0, -1};
    char dcs[] = {'R', 'D', 'L', 'U'};
    
    int iterations = 0;
    bool solvingForRule = (targetNoun != -1);
    int maxPushesLogged = -1;

    while(!pq.empty()) {
        iterations++;
        if (iterations > maxIterations) break;

        StateNode current = pq.top(); pq.pop();
        GameState state = current.state;
        std::string path = current.path;

        if (current.pushes > maxPushesLogged) {
            maxPushesLogged = current.pushes;
            std::cout << "Pushes: " << maxPushesLogged 
                      << " | Queue: " << pq.size() 
                      << " | Visited: " << visited.size() << std::endl;
        }

        // CHECK GOAL
        if (solvingForRule) {
            if ((state.propertyMap[TextToElement(targetNoun)] & TextToProp(targetProp)) != 0) return path;
        } else {
            if (state.hasWon) return path;
        }
        
        // Canonicalize
        GameState canon = state;
        CanonicalizeState(canon);
        std::string hash = SerializeState(canon);
        if (visited.find(hash) != visited.end()) continue;
        visited.insert(hash);

        std::pair<int, int> p = FindPlayerPos(state);
        int px = p.first; int py = p.second;
        if (px == -1) continue;
        
        auto reachable = GetReachableCells(state, {{px, py}});
        std::vector<std::pair<int,int>> pushableSources;

        for(int y=0; y<currentHeight; y++) {
            for(int x=0; x<currentWidth; x++) {
                if(reachable[y*currentWidth+x]) {
                    // Check WIN
                    if (!solvingForRule) {
                        const Cell& c = GetCell(const_cast<GameState&>(state), x, y);
                        for(const auto& obj : c.objects) {
                            if(HasProp(const_cast<GameState&>(state), obj.element, P_WIN)) {
                                return path + GetWalkPath(state, px, py, x, y);
                            }
                        }
                    }
                    pushableSources.push_back({x, y});
                }
            }
        }
        
        // GENERATE PUSH MOVES
        for(auto& pos : pushableSources) {
            int rx = pos.first; int ry = pos.second;
            for(int i=0; i<4; i++) {
                int dx = dxs[i], dy = dys[i];
                int tx = rx + dx, ty = ry + dy;
                if(tx < 0 || tx >= currentWidth || ty < 0 || ty >= currentHeight) continue;
                
                bool isPush = false;
                const Cell& target = GetCell(const_cast<GameState&>(state), tx, ty);
                for(const auto& obj : target.objects) {
                    if(HasProp(const_cast<GameState&>(state), obj.element, P_PUSH)) { isPush = true; break; }
                }
                
                if(isPush && CanMove(const_cast<GameState&>(state), rx, ry, dx, dy)) {
                    GameState nextState = state;
                    TeleportPlayer(nextState, px, py, rx, ry);
                    nextState = MakeMove(nextState, dx, dy);
                    
                    if (FindPlayerPos(nextState).first == -1) continue;

                    // 2. Calculate New Heuristic for Child Node
                    int newH = GetHeuristic(nextState, targetNoun, targetProp);
                    std::string walk = GetWalkPath(state, px, py, rx, ry);
                    if (walk.empty() && (px != rx || py != ry)) continue;
                    
                    pq.push({nextState, path + walk + dcs[i], current.pushes + 1, newH});
                }
            }
        }
    }
    return "";
}

// --- REACHABILITY HELPERS ---

// Returns a boolean mask of cells where the block at (bx, by) can be pushed to.
static std::vector<bool> GetPushableReach(const GameState& state, int bx, int by) {
    std::vector<bool> result(currentWidth * currentHeight, false);
    
    // 1. Check if object is pushable
    bool isPush = false;
    const Cell& c = GetCell(const_cast<GameState&>(state), bx, by);
    for(const auto& o : c.objects) if(HasProp(const_cast<GameState&>(state), o.element, P_PUSH)) isPush = true;
    if(!isPush) return result;

    // 2. Find Players
    auto players = FindAllPlayerPos(state);
    if(players.empty()) return result;

    // 3. BFS
    // State: BoxPos (int), CanonicalPlayerPos (int)
    std::set<std::pair<int, int>> visited;
    struct Node { int bx, by; int px, py; };
    std::queue<Node> q;

    // Initial Reachability
    std::vector<bool> initialReachable = GetReachableCells(state, players);
    
    // Find canonical player pos (first reachable cell)
    int initialCanP = -1;
    for(int i=0; i<currentWidth*currentHeight; i++) {
        if(initialReachable[i]) { initialCanP = i; break; }
    }
    if(initialCanP == -1) return result; 
    
    result[by * currentWidth + bx] = true;
    
    int pX = initialCanP % currentWidth;
    int pY = initialCanP / currentWidth;
    
    q.push({bx, by, pX, pY});
    visited.insert({by * currentWidth + bx, initialCanP});

    int dx[] = {1, -1, 0, 0};
    int dy[] = {0, 0, 1, -1};
    
    bool isMelt = false;
    for(int i=0; i<100; i++) if ((state.propertyMap[i] & P_YOU) && (state.propertyMap[i] & P_MELT)) isMelt = true;

    while(!q.empty()) {
        Node curr = q.front(); q.pop();
        
        // Compute reachable area for player with box at curr.bx, curr.by
        std::vector<bool> pReachable(currentWidth * currentHeight, false);
        std::queue<std::pair<int,int>> pq;
        
        if (curr.px >= 0 && curr.px < currentWidth && curr.py >= 0 && curr.py < currentHeight) {
            pq.push({curr.px, curr.py});
            pReachable[curr.py * currentWidth + curr.px] = true;
        }
        
        while(!pq.empty()) {
            auto [cx, cy] = pq.front(); pq.pop();
            for(int i=0; i<4; i++) {
                int nx = cx + dx[i], ny = cy + dy[i];
                if(nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight) {
                    if(!pReachable[ny * currentWidth + nx]) {
                        if(nx == curr.bx && ny == curr.by) continue; // Blocked by current box
                        
                        bool walkable = true;
                        if (nx == bx && ny == by) {
                            // Original box position: check if OTHER objects block
                            const Cell& cell = GetCell(const_cast<GameState&>(state), nx, ny);
                            for(const auto& obj : cell.objects) {
                                if(HasProp(const_cast<GameState&>(state), obj.element, P_PUSH)) continue; 
                                if(HasProp(const_cast<GameState&>(state), obj.element, P_STOP) || 
                                   HasProp(const_cast<GameState&>(state), obj.element, P_SINK) || 
                                   HasProp(const_cast<GameState&>(state), obj.element, P_DEFEAT)) {
                                    walkable = false; break;
                                }
                                if (isMelt && HasProp(const_cast<GameState&>(state), obj.element, P_HOT)) { walkable = false; break; }
                            }
                        } else {
                            walkable = IsWalkable(state, nx, ny, isMelt);
                        }
                        
                        if(walkable) {
                            pReachable[ny * currentWidth + nx] = true;
                            pq.push({nx, ny});
                        }
                    }
                }
            }
        }
        
        // Try Pushing
        for(int i=0; i<4; i++) {
            int pushX = curr.bx - dx[i];
            int pushY = curr.by - dy[i];
            
            if(pushX >= 0 && pushX < currentWidth && pushY >= 0 && pushY < currentHeight && pReachable[pushY * currentWidth + pushX]) {
                int destX = curr.bx + dx[i];
                int destY = curr.by + dy[i];
                
                if(destX >= 0 && destX < currentWidth && destY >= 0 && destY < currentHeight) {
                    bool destValid = true;
                    bool isSink = false;
                    
                    const Cell& destCell = GetCell(const_cast<GameState&>(state), destX, destY);
                    for(const auto& obj : destCell.objects) {
                        if (destX == bx && destY == by) {
                            if(HasProp(const_cast<GameState&>(state), obj.element, P_PUSH)) continue;
                        }
                        if(HasProp(const_cast<GameState&>(state), obj.element, P_STOP)) { destValid = false; break; }
                        if(HasProp(const_cast<GameState&>(state), obj.element, P_SINK)) { isSink = true; }
                    }
                    
                    if(destValid) {
                        if (isSink) {
                            result[destY * currentWidth + destX] = true;
                        } else {
                            // Calculate canonical player pos for new state
                            int minIdx = 100000;
                            std::queue<std::pair<int,int>> cpq;
                            std::vector<bool> cVis(currentWidth * currentHeight, false);
                            cpq.push({curr.bx, curr.by}); // Player moves to where box was
                            cVis[curr.by * currentWidth + curr.bx] = true;
                            
                            while(!cpq.empty()) {
                                auto [cx, cy] = cpq.front(); cpq.pop();
                                int idx = cy * currentWidth + cx;
                                if (idx < minIdx) minIdx = idx;
                                
                                for(int k=0; k<4; k++) {
                                    int nx = cx + dx[k], ny = cy + dy[k];
                                    if(nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight) {
                                        if(!cVis[ny*currentWidth+nx]) {
                                            if(nx == destX && ny == destY) continue;
                                            
                                            bool w = true;
                                            if(nx == bx && ny == by) {
                                                 const Cell& cell = GetCell(const_cast<GameState&>(state), nx, ny);
                                                 for(const auto& obj : cell.objects) {
                                                     if(HasProp(const_cast<GameState&>(state), obj.element, P_PUSH)) continue;
                                                     if(HasProp(const_cast<GameState&>(state), obj.element, P_STOP) || 
                                                        HasProp(const_cast<GameState&>(state), obj.element, P_SINK) || 
                                                        HasProp(const_cast<GameState&>(state), obj.element, P_DEFEAT)) {
                                                         w = false; break;
                                                     }
                                                     if (isMelt && HasProp(const_cast<GameState&>(state), obj.element, P_HOT)) { w = false; break; }
                                                 }
                                            } else {
                                                w = IsWalkable(state, nx, ny, isMelt);
                                            }
                                            
                                            if(w) {
                                                cVis[ny*currentWidth+nx] = true;
                                                cpq.push({nx, ny});
                                            }
                                        }
                                    }
                                }
                            }
                            
                            if(visited.find({destY * currentWidth + destX, minIdx}) == visited.end()) {
                                visited.insert({destY * currentWidth + destX, minIdx});
                                result[destY * currentWidth + destX] = true;
                                q.push({destX, destY, curr.bx, curr.by});
                            }
                        }
                    }
                }
            }
        }
    }
    return result;
}

// --- LOGIC SOLVER ---
struct Rule {
    int noun;
    int prop;
    std::string ToString() const { return std::to_string(noun) + "-" + std::to_string(prop); }
};

std::vector<Rule> GetPotentialRules(const GameState& s) {
    std::set<int> nouns, props;
    bool hasIs = false;
    for(const auto& cell : s.grid) {
        for(const auto& o : cell.objects) {
            if (IsNoun(o.element)) nouns.insert(o.element);
            if (IsProperty(o.element)) props.insert(o.element);
            if (o.element == TEXT_IS) hasIs = true;
        }
    }
    std::vector<Rule> potential;
    if (!hasIs) return potential;
    
    for (int n : nouns) {
        for (int p : props) {
            // FIX: Don't allow "X IS IS"
            if (p == TEXT_IS) continue; 
            potential.push_back({n, p});
        }
        // Transformation Rules (Noun IS Noun)
        for (int n2 : nouns) {
            potential.push_back({n, n2});
        }
    }
    
    // Sort to prioritize WIN and YOU
    std::sort(potential.begin(), potential.end(), [](const Rule& a, const Rule& b) {
        auto isImp = [](int p) { return p == TEXT_WIN || p == TEXT_YOU; };
        bool aImp = isImp(a.prop);
        bool bImp = isImp(b.prop);
        if (aImp != bImp) return aImp; 
        return a.noun < b.noun;
    });
    
    return potential;
}

std::string GetLogicHash(const GameState& s) {
    std::string hash = "";
    for(int i=0; i<100; i++) if(s.propertyMap[i]) hash += std::to_string(i) + ":" + std::to_string(s.propertyMap[i]) + "|";
    hash += "_TXT_";
    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            const Cell& c = GetCell(const_cast<GameState&>(s), x, y);
            for(const auto& o : c.objects) hash += std::to_string(o.element) + "@" + std::to_string(y*currentWidth+x) + ",";
        }
    }
    return hash;
}

bool IsRuleReachable(const GameState& s, int noun, int prop) {
    auto players = FindAllPlayerPos(s);
    if (players.empty()) return false;

    auto reachable = GetReachableCells(s, players);
    auto accessible = GetAccessibleCells(reachable);
    
    int required[] = {noun, TEXT_IS, prop};
    bool found[3] = {false, false, false};
    
    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            if (accessible[y*currentWidth+x]) {
                const Cell& c = GetCell(const_cast<GameState&>(s), x, y);
                for(const auto& o : c.objects) {
                    if (o.element == required[0]) found[0] = true;
                    if (o.element == required[1]) found[1] = true;
                    if (o.element == required[2]) found[2] = true;
                }
            }
        }
    }
    return found[0] && found[1] && found[2];
}

// --- HIGH LEVEL SOLVER HELPERS ---

struct RuleLoc {
    int noun, prop;
    int x1, y1, x2, y2, x3, y3; // Noun, IS, Prop coords
};

static std::vector<RuleLoc> GetRuleLocations(const GameState& state) {
    std::vector<RuleLoc> rules;
    // Horizontal
    for (int y = 0; y < currentHeight; y++) {
        for (int x = 0; x < currentWidth - 2; x++) {
            const Cell& mid = GetCell(const_cast<GameState&>(state), x+1, y);
            if (mid.objects.empty() || mid.objects[0].element != TEXT_IS) continue;
            const Cell& left = GetCell(const_cast<GameState&>(state), x, y);
            const Cell& right = GetCell(const_cast<GameState&>(state), x+2, y);
            if (!left.objects.empty() && !right.objects.empty()) {
                if (IsNoun(left.objects[0].element) && (IsProperty(right.objects[0].element) || IsNoun(right.objects[0].element))) {
                    rules.push_back({left.objects[0].element, right.objects[0].element, x, y, x+1, y, x+2, y});
                }
            }
        }
    }
    // Vertical
    for (int x = 0; x < currentWidth; x++) {
        for (int y = 0; y < currentHeight - 2; y++) {
            const Cell& mid = GetCell(const_cast<GameState&>(state), x, y+1);
            if (mid.objects.empty() || mid.objects[0].element != TEXT_IS) continue;
            const Cell& up = GetCell(const_cast<GameState&>(state), x, y);
            const Cell& down = GetCell(const_cast<GameState&>(state), x, y+2);
            if (!up.objects.empty() && !down.objects.empty()) {
                if (IsNoun(up.objects[0].element) && (IsProperty(down.objects[0].element) || IsNoun(down.objects[0].element))) {
                    rules.push_back({up.objects[0].element, down.objects[0].element, x, y, x, y+1, x, y+2});
                }
            }
        }
    }
    return rules;
}

LogicSolver::LogicSolver(const GameState& startState) {
    GameState initial = startState;
    ParseRules(initial); // Ensure propertyMap is populated
    q.push({initial, ""});
    visited.insert(GetLogicHash(initial));
}

std::vector<std::string> LogicSolver::ParsePlan(const std::string& plan) {
    std::vector<std::string> steps;
    std::string delim = "\n -> ";
    size_t start = 0;
    size_t end = plan.find(delim);
    
    // If plan starts with delim, skip the first empty part
    if (end == 0) {
        start = delim.length();
        end = plan.find(delim, start);
    }

    while (end != std::string::npos) {
        steps.push_back(plan.substr(start, end - start));
        start = end + delim.length();
        end = plan.find(delim, start);
    }
    if (start < plan.length()) {
        steps.push_back(plan.substr(start));
    }
    return steps;
}

bool LogicSolver::IsRedundant(const std::vector<std::string>& currentSteps) {
    for (const auto& oldSteps : foundPlans) {
        if (oldSteps.size() > currentSteps.size()) continue;
        
        size_t i = 0, j = 0;
        while (i < oldSteps.size() && j < currentSteps.size()) {
            if (oldSteps[i] == currentSteps[j]) i++;
            j++;
        }
        if (i == oldSteps.size()) return true;
    }
    return false;
}

std::string LogicSolver::NextSolution() {
    auto GetSolverName = [](int e) {
        std::string name = GetElementName(e);
        if (name.length() > 5 && name.substr(0, 5) == "TEXT_") {
            return name.substr(5);
        }
        return name;
    };

    int iterations = 0;
    std::cout << "--- Starting Logic Solver ---" << std::endl;

    while(!q.empty()) {
        iterations++;
        if (iterations > 5000) {
            std::cout << "Logic Solver Timeout reached." << std::endl;
            return "Logic Solver Timeout";
        }

        auto current = q.front(); q.pop();
        GameState& s = current.state;

        if (iterations % 100 == 0 || iterations == 1) {
            std::cout << "Logic Iteration " << iterations << " | Queue: " << q.size() << " | Plan Length: " << current.plan.length() << std::endl;
        }

        // 1. Check Win (Can YOU reach WIN?)
        // We use the floodfill from FindPlayerPos logic implicitly
        auto players = FindAllPlayerPos(s);
        bool winReachable = false;
        if (!players.empty()) {
            auto reachable = GetReachableCells(s, players);
            
            // Check WIN
            for(int y=0; y<currentHeight; y++) {
                for(int x=0; x<currentWidth; x++) {
                    if(reachable[y*currentWidth+x]) {
                        const Cell& c = GetCell(const_cast<GameState&>(s), x, y);
                        for(const auto& obj : c.objects) {
                            if(HasProp(s, obj.element, P_WIN)) {
                                winReachable = true;
                                if (!current.padding) {
                                    std::string sol = current.plan + "\n -> Reach " + GetElementName(obj.element);
                                    
                                    std::vector<std::string> steps = ParsePlan(sol);
                                    if (!IsRedundant(steps)) {
                                        foundPlans.push_back(steps);
                                        return sol;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // If we can already win from this state, don't generate further steps (avoid unnecessary padding)
            if (winReachable) continue;

            // Compute Accessibility (Reachable OR Adjacent to Reachable)
            // This allows us to interact with PUSH objects (like Text) that we can't walk ON but can walk NEXT to.
            auto accessible = GetAccessibleCells(reachable);
            
            auto IsPushable = [&](int x, int y) {
                bool isPush = false;
                const Cell& c = GetCell(const_cast<GameState&>(s), x, y);
                for(const auto& o : c.objects) if(HasProp(const_cast<GameState&>(s), o.element, P_PUSH)) isPush = true;
                if(!isPush) return false;

                int dx[] = {1, -1, 0, 0};
                int dy[] = {0, 0, 1, -1};
                for(int i=0; i<4; i++) {
                    int px = x - dx[i];
                    int py = y - dy[i];
                    if(px>=0 && px<currentWidth && py>=0 && py<currentHeight && reachable[py*currentWidth+px]) {
                        if(CanMove(const_cast<GameState&>(s), x, y, dx[i], dy[i])) return true;
                    }
                }
                return false;
            };

            // 2. Analyze Resources
            std::map<int, int> movableInventory;
            std::vector<Rule> fixedRules;
            std::vector<RuleLoc> currentLocs = GetRuleLocations(s);

            // Identify Critical Rules (Sole source of YOU)
            std::set<int> criticalCoords;
            int youRuleCount = 0;
            int criticalRuleIdx = -1;
            
            for(int i=0; i<currentLocs.size(); i++) {
                const auto& r = currentLocs[i];
                if (TextToProp(r.prop) == P_YOU) {
                    if (s.propertyMap[TextToElement(r.noun)] & P_YOU) {
                        youRuleCount++;
                        criticalRuleIdx = i;
                    }
                }
            }
            
            if (youRuleCount == 1) {
                const auto& r = currentLocs[criticalRuleIdx];
                criticalCoords.insert(r.y1 * currentWidth + r.x1);
                criticalCoords.insert(r.y2 * currentWidth + r.x2);
                criticalCoords.insert(r.y3 * currentWidth + r.x3);
            }
            
            struct ActiveRuleInfo {
                Rule rule;
                bool nounFixed;
                bool isFixed;
                bool propFixed;
            };
            std::vector<ActiveRuleInfo> activeRuleInfos;

            // Populate Movable Inventory (Only Pushable text that is NOT critical)
            std::map<int, std::vector<std::vector<bool>>> textReachability;
            for(int y=0; y<currentHeight; y++) {
                for(int x=0; x<currentWidth; x++) {
                    if (accessible[y*currentWidth+x]) {
                        const Cell& c = GetCell(s, x, y);
                        for(const auto& o : c.objects) {
                            if (o.element >= 10) {
                                if (IsPushable(x, y)) {
                                    if (criticalCoords.find(y*currentWidth+x) == criticalCoords.end()) {
                                        movableInventory[o.element]++;
                                        textReachability[o.element].push_back(GetPushableReach(s, x, y));
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Identify Fixed vs Mutable Rules
            std::vector<Rule> currentlyActive;

            for(auto& r : currentLocs) {
                bool nP = IsPushable(r.x1, r.y1);
                bool iP = IsPushable(r.x2, r.y2);
                bool pP = IsPushable(r.x3, r.y3);
                
                bool active = false;
                if (IsProperty(r.prop)) {
                    active = (s.propertyMap[TextToElement(r.noun)] & TextToProp(r.prop));
                } else {
                    int n = TextToElement(r.noun);
                    int p = TextToElement(r.prop);
                    for(const auto& tr : s.transformRules) 
                        if(tr.first == n && tr.second == p) { active = true; break; }
                }
                
                if (!nP && !iP && !pP) {
                    // Fully Fixed (Permanent)
                    if (active) fixedRules.push_back({r.noun, r.prop});
                } else {
                    // Partially or Fully Movable
                    if (active) {
                        currentlyActive.push_back({r.noun, r.prop});
                        activeRuleInfos.push_back({{r.noun, r.prop}, !nP, !iP, !pP});
                    }
                }
            }

            if (iterations == 1) {
                std::cout << "Initial Mutable Rules: " << currentlyActive.size() << std::endl;
                for(auto& r : currentlyActive) std::cout << " - " << GetSolverName(r.noun) << " IS " << GetSolverName(r.prop) << std::endl;
            }

            // Helper to apply rules and push state
            auto TryPushState = [&](const std::vector<Rule>& newMutableRules, const std::string& stepDesc) {
                GameState nextState = s;
                for(int i=0; i<100; i++) nextState.propertyMap[i] = 0;
                nextState.transformRules.clear();
                
                auto ApplyRule = [&](const Rule& r) {
                    if (IsProperty(r.prop)) {
                        nextState.propertyMap[TextToElement(r.noun)] |= TextToProp(r.prop);
                    } else if (IsNoun(r.prop)) {
                        nextState.transformRules.push_back({TextToElement(r.noun), TextToElement(r.prop)});
                    }
                };

                for(auto& r : fixedRules) ApplyRule(r);
                for(auto& r : newMutableRules) ApplyRule(r);
                
                ProcessTransformations(nextState);
                ProcessInteractions(nextState);
                
                bool hasYou = false;
                for(const auto& cell : nextState.grid) {
                    for(const auto& obj : cell.objects) {
                        if(HasProp(nextState, obj.element, P_YOU)) { hasYou = true; break; }
                    }
                    if(hasYou) break;
                }
                
                std::string h = GetLogicHash(nextState);
                if (hasYou && visited.find(h) == visited.end()) {
                    visited.insert(h);
                    q.push({nextState, current.plan + stepDesc});
                    std::cout << "  [Logic] " << stepDesc.substr(1) << std::endl; // substr(1) to skip newline
                }
            };

            // Helper to check resources (Movable Inventory + Fixed Slots)
            auto CheckResources = [&](const std::vector<Rule>& targetRules) {
                std::map<int, int> inv = movableInventory;
                std::vector<ActiveRuleInfo> slots = activeRuleInfos;
                
                for(const auto& tr : targetRules) {
                    bool satisfied = false;
                    
                    // 1. Try to use a Slot (Maintain or Adapt existing rule)
                    for(auto it = slots.begin(); it != slots.end(); ++it) {
                        bool match = true;
                        // If the slot has a fixed component, the target must match it
                        if (it->nounFixed && it->rule.noun != tr.noun) match = false;
                        if (it->propFixed && it->rule.prop != tr.prop) match = false;
                        
                        if (match) {
                            // Cost is 0 if we keep the existing component, 1 if we swap it
                            int costNoun = (it->rule.noun == tr.noun) ? 0 : 1;
                            int costIs   = 0; // IS is always IS
                            int costProp = (it->rule.prop == tr.prop) ? 0 : 1;
                            
                            if (inv[tr.noun] >= costNoun && inv[TEXT_IS] >= costIs && inv[tr.prop] >= costProp) {
                                inv[tr.noun] -= costNoun;
                                inv[TEXT_IS] -= costIs;
                                inv[tr.prop] -= costProp;
                                slots.erase(it);
                                satisfied = true;
                                break;
                            }
                        }
                    }
                    if (satisfied) continue;
                    
                    // 2. Build from Scratch (Purely movable)
                    if (inv[tr.noun] >= 1 && inv[TEXT_IS] >= 1 && inv[tr.prop] >= 1) {
                        inv[tr.noun]--; inv[TEXT_IS]--; inv[tr.prop]--;
                        satisfied = true;
                    }
                    
                    if (!satisfied) return false;
                }
                return true;
            };
            
            std::vector<Rule> potentialRules = GetPotentialRules(s); // All possible Noun-Prop pairs
            
            // STRATEGY 1: Break Active Rule
            for(size_t i=0; i<currentlyActive.size(); i++) {
                std::vector<Rule> nextRules = currentlyActive;
                nextRules.erase(nextRules.begin() + i);
                TryPushState(nextRules, "\n -> Break " + GetSolverName(currentlyActive[i].noun) + " IS " + GetSolverName(currentlyActive[i].prop));
            }
            
            // STRATEGY 2: Form New Rule (from Inventory)
            for(const auto& p : potentialRules) {
                // Check if p is already active
                bool active = false;
                for(const auto& ar : currentlyActive) if(ar.noun == p.noun && ar.prop == p.prop) active = true;
                if(active) continue;

                std::vector<Rule> nextRules = currentlyActive;
                nextRules.push_back(p);
                
                if(CheckResources(nextRules)) {
                    TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop));
                }
            }

            // STRATEGY 3: Form Cross-Rule (Intersecting Rules)
            // Reuse an existing text object on the board as part of a new rule
            for(const auto& p : potentialRules) {
                bool active = false;
                for(const auto& ar : currentlyActive) if(ar.noun == p.noun && ar.prop == p.prop) active = true;
                if(active) continue;

                for(const auto& r : currentLocs) {
                    int r_dx = r.x2 - r.x1;
                    int r_dy = r.y2 - r.y1;
                    
                    auto IsFree = [&](int x, int y) {
                        if (x < 0 || x >= currentWidth || y < 0 || y >= currentHeight) return false;
                        if (!accessible[y*currentWidth+x]) return false;
                        const Cell& c = GetCell(const_cast<GameState&>(s), x, y);
                        for(const auto& o : c.objects) {
                            if (HasProp(const_cast<GameState&>(s), o.element, P_STOP) && !HasProp(const_cast<GameState&>(s), o.element, P_PUSH)) return false;
                        }
                        return true;
                    };

                    auto CanReach = [&](int elem, int tx, int ty) {
                        if (textReachability.find(elem) == textReachability.end()) return false;
                        if (tx < 0 || tx >= currentWidth || ty < 0 || ty >= currentHeight) return false;
                        for(const auto& mask : textReachability[elem]) {
                            if (mask[ty * currentWidth + tx]) return true;
                        }
                        return false;
                    };

                    // Iterate canonical reading directions: Right (1,0) and Down (0,1)
                    int dirs[2][2] = {{1, 0}, {0, 1}};
                    
                    for(int i=0; i<2; i++) {
                        int dx = dirs[i][0];
                        int dy = dirs[i][1];
                        
                        // Determine relation to existing rule 'r'
                        // Parallel if direction matches r (or opposite, but we only check positive reading dirs)
                        bool isParallel = (dx == r_dx && dy == r_dy) || (dx == -r_dx && dy == -r_dy);
                        bool isPerp = !isParallel;

                        // 1. Share NOUN (r.noun == p.noun)
                        // New Rule: p.noun IS p.prop. Anchor: p.noun (r.x1, r.y1).
                        // Must build Forward (Away from anchor).
                        // Valid if Perpendicular. (Parallel Forward blocked by existing IS).
                        if (r.noun == p.noun && isPerp) {
                            int isX = r.x1 + dx, isY = r.y1 + dy;
                            int prX = r.x1 + 2*dx, prY = r.y1 + 2*dy;
                            
                            if (IsFree(isX, isY) && IsFree(prX, prY)) {
                                if (CanReach(TEXT_IS, isX, isY) && CanReach(p.prop, prX, prY)) {
                                    std::vector<Rule> nextRules = currentlyActive;
                                    nextRules.push_back(p);
                                    TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Cross-Noun)");
                                }
                            }
                        }

                        // 2. Share PROP (r.prop == p.prop)
                        // New Rule: p.noun IS p.prop. Anchor: p.prop (r.x3, r.y3).
                        // Must build Backward (Before anchor).
                        // Valid if Perpendicular. (Parallel Backward blocked by existing IS).
                        if (r.prop == p.prop && isPerp) {
                            int isX = r.x3 - dx, isY = r.y3 - dy;
                            int nX = r.x3 - 2*dx, nY = r.y3 - 2*dy;
                            
                            if (IsFree(isX, isY) && IsFree(nX, nY)) {
                                if (CanReach(TEXT_IS, isX, isY) && CanReach(p.noun, nX, nY)) {
                                    std::vector<Rule> nextRules = currentlyActive;
                                    nextRules.push_back(p);
                                    TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Cross-Prop)");
                                }
                            }
                        }

                        // 3. Share IS
                        // New Rule: p.noun IS p.prop. Anchor: IS (r.x2, r.y2).
                        // Noun at IS-d, Prop at IS+d.
                        // Valid if Perpendicular.
                        if (isPerp) {
                            int nX = r.x2 - dx, nY = r.y2 - dy;
                            int prX = r.x2 + dx, prY = r.y2 + dy;
                            
                            if (IsFree(nX, nY) && IsFree(prX, prY)) {
                                if (CanReach(p.noun, nX, nY) && CanReach(p.prop, prX, prY)) {
                                    std::vector<Rule> nextRules = currentlyActive;
                                    nextRules.push_back(p);
                                    TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Cross-IS)");
                                }
                            }
                        }

                        // 4. Share Noun as Prop (r.noun == p.prop)
                        // New Rule: p.noun IS p.prop (where p.prop is r.noun).
                        // Anchor: r.noun (r.x1, r.y1).
                        // Must build Backward (Before anchor).
                        // Valid if Perpendicular OR Parallel (Chaining: X IS A IS B).
                        if (r.noun == p.prop) {
                            int isX = r.x1 - dx, isY = r.y1 - dy;
                            int nX = r.x1 - 2*dx, nY = r.y1 - 2*dy;
                            
                            if (IsFree(isX, isY) && IsFree(nX, nY)) {
                                if (CanReach(TEXT_IS, isX, isY) && CanReach(p.noun, nX, nY)) {
                                    std::vector<Rule> nextRules = currentlyActive;
                                    nextRules.push_back(p);
                                    TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Chain-Into-Noun)");
                                }
                            }
                        }

                        // 5. Share Prop as Noun (r.prop == p.noun)
                        // New Rule: p.noun IS p.prop (where p.noun is r.prop).
                        // Anchor: r.prop (r.x3, r.y3).
                        // Must build Forward (After anchor).
                        // Valid if Perpendicular OR Parallel (Chaining: A IS B IS C).
                        if (r.prop == p.noun) {
                            int isX = r.x3 + dx, isY = r.y3 + dy;
                            int prX = r.x3 + 2*dx, prY = r.y3 + 2*dy;
                            
                            if (IsFree(isX, isY) && IsFree(prX, prY)) {
                                if (CanReach(TEXT_IS, isX, isY) && CanReach(p.prop, prX, prY)) {
                                    std::vector<Rule> nextRules = currentlyActive;
                                    nextRules.push_back(p);
                                    TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Chain-From-Prop)");
                                }
                            }
                        }
                    }
                }
            }
            
            // STRATEGY 4: Swap Active -> New
            for(size_t i=0; i<currentlyActive.size(); i++) {
                for(const auto& p : potentialRules) {
                    bool active = false;
                    for(const auto& ar : currentlyActive) if(ar.noun == p.noun && ar.prop == p.prop) active = true;
                    if(active) continue;
                    
                    if (currentlyActive[i].noun == p.noun && currentlyActive[i].prop == p.prop) continue;

                    std::vector<Rule> nextRules = currentlyActive;
                    nextRules.erase(nextRules.begin() + i);
                    nextRules.push_back(p);
                    
                    if(CheckResources(nextRules)) {
                         TryPushState(nextRules, "\n -> Break " + GetSolverName(currentlyActive[i].noun) + " IS " + GetSolverName(currentlyActive[i].prop) + 
                                                 ", Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop));
                    }
                }
            }

            // STRATEGY 5: Neutralize SINK or HAZARD
            // 1. Find reachable PUSH objects (Ammo)
            struct AmmoInfo { int x, y; int elem; bool isSink; };
            std::vector<AmmoInfo> ammoList;
            
            for(int y=0; y<currentHeight; y++) {
                for(int x=0; x<currentWidth; x++) {
                    if(accessible[y*currentWidth+x]) {
                        const Cell& c = GetCell(s, x, y);
                        for(const auto& o : c.objects) {
                            if(HasProp(s, o.element, P_PUSH)) {
                                bool isSink = HasProp(s, o.element, P_SINK);
                                ammoList.push_back({x, y, o.element, isSink});
                            }
                        }
                    }
                }
            }

            bool playerIsMelt = false;
            for(int i=0; i<100; i++) if((s.propertyMap[i] & P_YOU) && (s.propertyMap[i] & P_MELT)) playerIsMelt = true;

            // 2. Find Targets (Sink or Hazard)
            if (!ammoList.empty()) {
                std::vector<std::vector<bool>> ammoReach(ammoList.size());

                for(int y=0; y<currentHeight; y++) {
                    for(int x=0; x<currentWidth; x++) {
                        const Cell& c = GetCell(s, x, y);
                        
                        bool isSinkTarget = false;
                        bool isHazardTarget = false;
                        int targetElem = -1;

                        for(const auto& o : c.objects) {
                            if(HasProp(s, o.element, P_SINK)) {
                                isSinkTarget = true;
                                targetElem = o.element;
                                break;
                            }
                            if(HasProp(s, o.element, P_DEFEAT)) {
                                isHazardTarget = true;
                                targetElem = o.element;
                                break;
                            }
                            if(playerIsMelt && HasProp(s, o.element, P_HOT)) {
                                isHazardTarget = true;
                                targetElem = o.element;
                                break;
                            }
                        }
                        
                        if(isSinkTarget || isHazardTarget) {
                            int bestAmmoIdx = -1;
                            for(int k=0; k<ammoList.size(); k++) {
                                if (ammoList[k].x == x && ammoList[k].y == y) continue; // Don't push into self
                                if (isHazardTarget && !ammoList[k].isSink) continue; // Hazards require Sink Ammo
                                
                                if (ammoReach[k].empty()) ammoReach[k] = GetPushableReach(s, ammoList[k].x, ammoList[k].y);
                                
                                if (ammoReach[k][y * currentWidth + x]) {
                                    bestAmmoIdx = k;
                                    break;
                                }
                            }
                            
                            if(bestAmmoIdx != -1) {
                                GameState nextState = s;
                                // Remove Target
                                Cell& targetCell = GetCell(nextState, x, y);
                                for(auto it=targetCell.objects.begin(); it!=targetCell.objects.end(); ) { 
                                    if(it->element == targetElem) { it=targetCell.objects.erase(it); break; } else ++it; 
                                }
                                
                                // Remove Ammo
                                const auto& a = ammoList[bestAmmoIdx];
                                Cell& ammoCell = GetCell(nextState, a.x, a.y);
                                for(auto it=ammoCell.objects.begin(); it!=ammoCell.objects.end(); ) { 
                                    if(it->element == a.elem) { it=ammoCell.objects.erase(it); break; } else ++it; 
                                }
                                
                                std::string h = GetLogicHash(nextState);
                                if(visited.find(h) == visited.end()) {
                                    visited.insert(h);
                                    std::string action = isSinkTarget ? " into " : " to neutralize ";
                                    q.push({nextState, current.plan + "\n -> Push " + GetElementName(a.elem) + action + GetElementName(targetElem)});
                                    std::cout << "  [Logic] Push " << GetElementName(a.elem) << action << GetElementName(targetElem) << std::endl;
                                }
                            }
                        }
                    }
                }
            }

            // STRATEGY 6: Clear Blockage (Push Obstacle)
            // Look for lines of pushable objects where one side is reachable but the opposite side isn't.
            // Then push until the opposite side is reached and area is opened.
            for(int y=0; y<currentHeight; y++) {
                for(int x=0; x<currentWidth; x++) {
                    if(reachable[y*currentWidth+x]) {
                        int dx[] = {1, -1, 0, 0};
                        int dy[] = {0, 0, 1, -1};
                        
                        for(int i=0; i<4; i++) {
                            // 1. Identify Stack
                            int tx = x + dx[i];
                            int ty = y + dy[i];
                            int len = 0;
                            int firstPushElem = -1;
                            
                            while(tx >= 0 && tx < currentWidth && ty >= 0 && ty < currentHeight) {
                                const Cell& c = GetCell(s, tx, ty);
                                bool isPush = false;
                                for(const auto& o : c.objects) {
                                    if(HasProp(s, o.element, P_PUSH)) {
                                        isPush = true;
                                        if(len == 0) firstPushElem = o.element;
                                    }
                                }
                                if(isPush) {
                                    len++;
                                    tx += dx[i];
                                    ty += dy[i];
                                } else {
                                    break;
                                }
                            }
                            
                            if(len == 0) continue;
                            
                            // tx, ty is now the cell AFTER the stack (The "Opposite Side")
                            int targetX = tx;
                            int targetY = ty;
                            
                            // Check bounds
                            if(targetX < 0 || targetX >= currentWidth || targetY < 0 || targetY >= currentHeight) continue;
                            
                            // Check if opposite side is ALREADY reachable (if so, no need to clear this specific line)
                            if(reachable[targetY*currentWidth + targetX]) continue;
                            
                            // Try pushing
                            if(CanMove(const_cast<GameState&>(s), x, y, dx[i], dy[i])) {
                                GameState nextState = s;
                                
                                // Teleport player to the pushing position (x, y)
                                std::pair<int, int> pPos = FindPlayerPos(nextState);
                                if (pPos.first != -1) {
                                    TeleportPlayer(nextState, pPos.first, pPos.second, x, y);
                                }

                                int pushCount = 0;
                                int px = x, py = y;
                                
                                while(true) {
                                    DoPush(nextState, px, py, dx[i], dy[i]);
                                    
                                    // Move Player
                                    int npx = px + dx[i];
                                    int npy = py + dy[i];
                                    Cell& src = GetCell(nextState, px, py);
                                    Cell& dst = GetCell(nextState, npx, npy);
                                    bool playerMoved = false;
                                    int playerElem = -1;
                                    for(auto it = src.objects.begin(); it != src.objects.end(); ) {
                                        if(HasProp(nextState, it->element, P_YOU)) {
                                            playerElem = it->element;
                                            dst.objects.push_back(*it);
                                            it = src.objects.erase(it);
                                            playerMoved = true;
                                            break; 
                                        } else ++it;
                                    }
                                    
                                    if(!playerMoved) break;

                                    // Check for hazards (DEFEAT / HOT / SINK)
                                    bool died = false;
                                    bool isMelt = (playerElem != -1) && HasProp(nextState, playerElem, P_MELT);
                                    for(const auto& obj : dst.objects) {
                                        if (obj.element == playerElem) continue;
                                        if (HasProp(nextState, obj.element, P_DEFEAT)) { died = true; break; }
                                        if (HasProp(nextState, obj.element, P_SINK)) { died = true; break; }
                                        if (isMelt && HasProp(nextState, obj.element, P_HOT)) { died = true; break; }
                                    }
                                    if (died) break;

                                    px = npx; py = npy;
                                    pushCount++;

                                    std::string h = GetLogicHash(nextState);
                                    if(visited.find(h) == visited.end()) {
                                        auto newPlayers = FindAllPlayerPos(nextState);
                                        auto newReachable = GetReachableCells(nextState, newPlayers);
                                        
                                        // Condition 1: Opposite side (targetX, targetY) is now reachable
                                        if(newReachable[targetY*currentWidth + targetX]) {
                                            // Condition 2: At least one space around it is reachable
                                            bool areaOpen = false;
                                            int ndx[] = {1, -1, 0, 0};
                                            int ndy[] = {0, 0, 1, -1};
                                            for(int k=0; k<4; k++) {
                                                int nx = targetX + ndx[k];
                                                int ny = targetY + ndy[k];
                                                if(nx>=0 && nx<currentWidth && ny>=0 && ny<currentHeight) {
                                                    if(newReachable[ny*currentWidth+nx]) {
                                                        areaOpen = true;
                                                        break;
                                                    }
                                                }
                                            }
                                            
                                            if(areaOpen) {
                                                visited.insert(h);
                                                q.push({nextState, current.plan + "\n -> Push " + GetElementName(firstPushElem) + " to clear path"});
                                                std::cout << "  [Logic] Push " << GetElementName(firstPushElem) << " x" << pushCount << " cleared path to (" << targetX << "," << targetY << ")" << std::endl;
                                                break; 
                                            }
                                        }
                                    }
                                    
                                    // Check if we can continue pushing in the same direction
                                    if(!CanMove(nextState, px, py, dx[i], dy[i])) break;
                                    if(pushCount >= 10) break; // Avoid infinite loops
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return "No Logic Solution";
}