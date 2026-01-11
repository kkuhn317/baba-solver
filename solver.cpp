#include "solver.h"
#include "game.h"
#include <queue>
#include <unordered_set>
#include <tuple>
#include <iostream>
#include <utility>
#include <algorithm>
#include <functional>
#include <set>
#include <cmath> // Required for abs()

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

// --- OPTIMIZED LOGIC SOLVER ---

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

// Helper: BFS for pathfinding (walking only)
static std::string GetWalkPath(const GameState& state, int sx, int sy, int ex, int ey) {
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
            int nx = curr.x + dx[i], ny = curr.y + dy[i];
            if(nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight) {
                if(!visited[ny * currentWidth + nx]) {
                    bool blocked = false;
                    const Cell& c = GetCell(const_cast<GameState&>(state), nx, ny);
                    for(const auto& obj : c.objects) {
                        if(HasProp(const_cast<GameState&>(state), obj.element, P_STOP) || HasProp(const_cast<GameState&>(state), obj.element, P_PUSH)) blocked = true;
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
    
    std::vector<bool> visited(currentWidth * currentHeight, false);
    std::queue<std::pair<int,int>> q;
    q.push({px, py});
    visited[py * currentWidth + px] = true;
    
    int minX = px, minY = py;
    int dx[] = {1, -1, 0, 0}; int dy[] = {0, 0, 1, -1};
    
    while(!q.empty()) {
        auto [cx, cy] = q.front(); q.pop();
        if (cy < minY || (cy == minY && cx < minX)) { minX = cx; minY = cy; }
        
        for(int i=0; i<4; i++) {
            int nx = cx + dx[i], ny = cy + dy[i];
            if(nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight && !visited[ny*currentWidth+nx]) {
                bool blocked = false;
                const Cell& c = GetCell(state, nx, ny);
                for(const auto& obj : c.objects) {
                    if(HasProp(state, obj.element, P_STOP) || HasProp(state, obj.element, P_PUSH)) blocked = true;
                }
                if(!blocked) {
                    visited[ny*currentWidth+nx] = true;
                    q.push({nx, ny});
                }
            }
        }
    }
    TeleportPlayer(state, px, py, minX, minY);
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

    // Priority Queue sorts by F-Score (g + h)
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
            std::cout << "Push Depth: " << maxPushesLogged 
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
        
        // FIND PUSHABLE OBJECTS (BFS)
        std::vector<bool> reach(currentWidth * currentHeight, false);
        std::queue<std::pair<int,int>> fq;
        fq.push({px, py});
        reach[py*currentWidth+px] = true;
        std::vector<std::pair<int,int>> reachable;
        
        while(!fq.empty()) {
            auto [cx, cy] = fq.front(); fq.pop();
            reachable.push_back({cx, cy});
            
            // Optimization: If solving for WIN, check if we can WALK to the win
            if (!solvingForRule) {
                const Cell& c = GetCell(const_cast<GameState&>(state), cx, cy);
                for(const auto& obj : c.objects) {
                    if(HasProp(const_cast<GameState&>(state), obj.element, P_WIN)) {
                        return path + GetWalkPath(state, px, py, cx, cy);
                    }
                }
            }
            
            for(int i=0; i<4; i++) {
                int nx = cx + dxs[i], ny = cy + dys[i];
                if(nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight) {
                    if(!reach[ny*currentWidth+nx]) {
                        bool blocked = false;
                        const Cell& c = GetCell(const_cast<GameState&>(state), nx, ny);
                        for(const auto& obj : c.objects) {
                            if(HasProp(const_cast<GameState&>(state), obj.element, P_STOP) || HasProp(const_cast<GameState&>(state), obj.element, P_PUSH)) blocked = true;
                        }
                        if(!blocked) {
                            reach[ny*currentWidth+nx] = true;
                            fq.push({nx, ny});
                        }
                    }
                }
            }
        }
        
        // GENERATE PUSH MOVES
        for(auto& pos : reachable) {
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
            for(const auto& o : c.objects) if (o.element >= 10) hash += std::to_string(o.element) + "@" + std::to_string(y*currentWidth+x) + ",";
        }
    }
    return hash;
}

bool IsRuleReachable(const GameState& s, int noun, int prop) {
    auto [px, py] = FindPlayerPos(s);
    if (px == -1) return false;

    std::vector<bool> reachable(currentWidth * currentHeight, false);
    std::queue<std::pair<int,int>> q;
    q.push({px, py});
    reachable[py*currentWidth+px] = true;
    
    int dx[] = {1, -1, 0, 0}; int dy[] = {0, 0, 1, -1};
    
    while(!q.empty()) {
        auto [cx, cy] = q.front(); q.pop();
        for(int i=0; i<4; i++) {
            int nx = cx + dx[i], ny = cy + dy[i];
            if(nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight) {
                if(!reachable[ny*currentWidth+nx]) {
                    bool blocked = false;
                    const Cell& c = GetCell(const_cast<GameState&>(s), nx, ny);
                    for(const auto& o : c.objects) {
                         if (HasProp(const_cast<GameState&>(s), o.element, P_STOP) || HasProp(const_cast<GameState&>(s), o.element, P_PUSH)) blocked = true;
                    }
                    if(!blocked) {
                        reachable[ny*currentWidth+nx] = true;
                        q.push({nx, ny});
                    }
                }
            }
        }
    }
    
    int required[] = {noun, TEXT_IS, prop};
    bool found[3] = {false, false, false};
    
    for(int y=0; y<currentHeight; y++) {
        for(int x=0; x<currentWidth; x++) {
            bool accessible = false;
            if (reachable[y*currentWidth+x]) accessible = true;
            else {
                for(int i=0; i<4; i++) {
                    int nx = x+dx[i], ny = y+dy[i];
                    if(nx>=0 && nx<currentWidth && ny>=0 && ny<currentHeight && reachable[ny*currentWidth+nx]) {
                        accessible = true; break;
                    }
                }
            }
            if (accessible) {
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

static std::string GetElementName(int e) {
    switch(e) {
        case TEXT_BABA: return "BABA";
        case TEXT_FLAG: return "FLAG";
        case TEXT_WALL: return "WALL";
        case TEXT_ROCK: return "ROCK";
        case TEXT_YOU:  return "YOU";
        case TEXT_WIN:  return "WIN";
        case TEXT_STOP: return "STOP";
        case TEXT_PUSH: return "PUSH";
        default: return std::to_string(e);
    }
}

std::string SolveLogic(const GameState& startState) {
    struct LogicNode { GameState state; std::string fullPath; };
    std::queue<LogicNode> q;
    std::unordered_set<std::string> visited;

    q.push({startState, ""});
    visited.insert(GetLogicHash(startState));
    
    int iterations = 0;
    while(!q.empty()) {
        iterations++;
        auto current = q.front(); q.pop();
        std::cout << "Logic Step " << iterations << " (Path: " << current.fullPath.length() << ")\n";

        // 1. Try Win (A* makes this fast now!)
        std::string winPath = SolveOptimized(current.state, -1, -1, 10000);
        if (!winPath.empty()) return current.fullPath + winPath;

        // 2. Try Rules
        for (const auto& r : GetPotentialRules(current.state)) {
            if ((current.state.propertyMap[TextToElement(r.noun)] & TextToProp(r.prop)) != 0) continue;
            
            if (!IsRuleReachable(current.state, r.noun, r.prop)) continue;

            std::string rulePath = SolveOptimized(current.state, r.noun, r.prop, 3000);
            
            if (!rulePath.empty()) {
                GameState nextState = current.state;
                for(char c : rulePath) {
                    int dx=0, dy=0;
                    if(c=='U') dy=-1; if(c=='D') dy=1; if(c=='L') dx=-1; if(c=='R') dx=1;
                    nextState = MakeMove(nextState, dx, dy);
                }
                std::string h = GetLogicHash(nextState);
                if (visited.find(h) == visited.end()) {
                    visited.insert(h);
                    q.push({nextState, current.fullPath + rulePath});
                    std::cout << "  -> Formed " << GetElementName(r.noun) << " IS " << GetElementName(r.prop) << "\n";
                }
            }
        }
    }
    return "No Logic Solution";
}