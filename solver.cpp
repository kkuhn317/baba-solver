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

// --- HELPERS ---

// Checks if a cell is safe to walk on (not blocked, not a hazard)
static bool IsWalkable(const GameState& state, int x, int y) {
    GameState& s = const_cast<GameState&>(state);
    const Cell& c = GetCell(s, x, y);
    for(const auto& obj : c.objects) {
        if(HasProp(s, obj.element, P_STOP) || 
           HasProp(s, obj.element, P_PUSH) || 
           HasProp(s, obj.element, P_SINK) || 
           HasProp(s, obj.element, P_DEFEAT)) {
            return false;
        }
    }
    return true;
}

// Returns a boolean mask of all cells reachable by walking from (sx, sy)
static std::vector<bool> GetReachableCells(const GameState& state, const std::vector<std::pair<int, int>>& starts) {
    std::vector<bool> visited(currentWidth * currentHeight, false);
    std::queue<std::pair<int,int>> q;

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
                if(!visited[ny * currentWidth + nx] && IsWalkable(state, nx, ny)) {
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
                    if(IsWalkable(state, nx, ny)) {
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
                if (IsNoun(left.objects[0].element) && IsProperty(right.objects[0].element)) {
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
                if (IsNoun(up.objects[0].element) && IsProperty(down.objects[0].element)) {
                    rules.push_back({up.objects[0].element, down.objects[0].element, x, y, x, y+1, x, y+2});
                }
            }
        }
    }
    return rules;
}

std::string SolveLogic(const GameState& startState) {
    struct LogicNode { 
        GameState state; 
        std::string plan; 
    };
    
    std::queue<LogicNode> q;
    std::unordered_set<std::string> visited;

    // Initial State
    GameState initial = startState;
    ParseRules(initial); // Ensure propertyMap is populated
    q.push({initial, ""});
    visited.insert(GetLogicHash(initial));
    
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
        if (!players.empty()) {
            auto reachable = GetReachableCells(s, players);
            
            // Check WIN
            for(int y=0; y<currentHeight; y++) {
                for(int x=0; x<currentWidth; x++) {
                    if(reachable[y*currentWidth+x]) {
                        const Cell& c = GetCell(const_cast<GameState&>(s), x, y);
                        for(const auto& obj : c.objects) {
                            if(HasProp(s, obj.element, P_WIN)) {
                                return current.plan + " -> Reach WIN!";
                            }
                        }
                    }
                }
            }

            // Compute Accessibility (Reachable OR Adjacent to Reachable)
            // This allows us to interact with PUSH objects (like Text) that we can't walk ON but can walk NEXT to.
            auto accessible = GetAccessibleCells(reachable);

            // 2. Analyze Resources
            std::map<int, int> inventory;
            std::vector<Rule> fixedRules;
            std::vector<RuleLoc> currentLocs = GetRuleLocations(s);

            // Populate Inventory (All accessible text)
            for(int y=0; y<currentHeight; y++) {
                for(int x=0; x<currentWidth; x++) {
                    if (accessible[y*currentWidth+x]) {
                        const Cell& c = GetCell(s, x, y);
                        for(const auto& o : c.objects) {
                            if (o.element >= 10) inventory[o.element]++;
                        }
                    }
                }
            }

            // Identify Fixed vs Mutable Rules
            std::vector<Rule> currentlyActive;
            std::vector<Rule> currentlyBroken;

            for(auto& r : currentLocs) {
                bool r1 = accessible[r.y1*currentWidth+r.x1];
                bool r2 = accessible[r.y2*currentWidth+r.x2];
                bool r3 = accessible[r.y3*currentWidth+r.x3];
                
                if (r1 && r2 && r3) {
                    // It's mutable. Check if it's currently active in the state.
                    if (s.propertyMap[TextToElement(r.noun)] & TextToProp(r.prop)) {
                        currentlyActive.push_back({r.noun, r.prop});
                    } else {
                        currentlyBroken.push_back({r.noun, r.prop});
                    }
                } else {
                    // It's fixed (unreachable).
                    fixedRules.push_back({r.noun, r.prop});
                }
            }

            if (iterations == 1) {
                std::cout << "Initial Mutable Rules: " << currentlyActive.size() << std::endl;
                for(auto& r : currentlyActive) std::cout << " - " << GetElementName(r.noun) << " IS " << GetElementName(r.prop) << std::endl;
            }

            // Helper to apply rules and push state
            auto TryPushState = [&](const std::vector<Rule>& newMutableRules, const std::string& stepDesc) {
                GameState nextState = s;
                for(int i=0; i<100; i++) nextState.propertyMap[i] = 0;
                
                for(auto& r : fixedRules) nextState.propertyMap[TextToElement(r.noun)] |= TextToProp(r.prop);
                for(auto& r : newMutableRules) nextState.propertyMap[TextToElement(r.noun)] |= TextToProp(r.prop);
                
                bool hasYou = false;
                for (int i = 0; i < 100; i++) if (nextState.propertyMap[i] & P_YOU) hasYou = true;
                
                std::string h = GetLogicHash(nextState);
                if (hasYou && visited.find(h) == visited.end()) {
                    visited.insert(h);
                    q.push({nextState, current.plan + stepDesc});
                    std::cout << "  [Logic] " << stepDesc.substr(1) << std::endl; // substr(1) to skip newline
                }
            };

            // Helper to check inventory
            auto CheckInventory = [&](const std::vector<Rule>& rules) {
                std::map<int, int> tempInv = inventory;
                for(const auto& r : rules) {
                    if (tempInv[r.noun] > 0 && tempInv[TEXT_IS] > 0 && tempInv[r.prop] > 0) {
                        tempInv[r.noun]--;
                        tempInv[TEXT_IS]--;
                        tempInv[r.prop]--;
                    } else {
                        return false;
                    }
                }
                return true;
            };
            
            std::vector<Rule> potentialRules = GetPotentialRules(s); // All possible Noun-Prop pairs
            
            // STRATEGY 1: Break Active Rule
            for(size_t i=0; i<currentlyActive.size(); i++) {
                std::vector<Rule> nextRules = currentlyActive;
                nextRules.erase(nextRules.begin() + i);
                TryPushState(nextRules, "\n -> Break " + GetElementName(currentlyActive[i].noun) + " IS " + GetElementName(currentlyActive[i].prop));
            }
            
            // STRATEGY 2: Form New Rule (from Inventory)
            for(const auto& p : potentialRules) {
                // Check if p is already active
                bool active = false;
                for(const auto& ar : currentlyActive) if(ar.noun == p.noun && ar.prop == p.prop) active = true;
                if(active) continue;

                std::vector<Rule> nextRules = currentlyActive;
                nextRules.push_back(p);
                
                if(CheckInventory(nextRules)) {
                    TryPushState(nextRules, "\n -> Form " + GetElementName(p.noun) + " IS " + GetElementName(p.prop));
                }
            }

            // STRATEGY 3: Reform Broken Rule (from Grid)
            for(const auto& r : currentlyBroken) {
                std::vector<Rule> nextRules = currentlyActive;
                nextRules.push_back(r);
                if(CheckInventory(nextRules)) {
                    TryPushState(nextRules, "\n -> Reform " + GetElementName(r.noun) + " IS " + GetElementName(r.prop));
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
                    
                    if(CheckInventory(nextRules)) {
                         TryPushState(nextRules, "\n -> Break " + GetElementName(currentlyActive[i].noun) + " IS " + GetElementName(currentlyActive[i].prop) + 
                                                 ", Form " + GetElementName(p.noun) + " IS " + GetElementName(p.prop));
                    }
                }
            }
            
            // STRATEGY 5: Swap Active -> Broken (Reform)
            for(size_t i=0; i<currentlyActive.size(); i++) {
                for(const auto& r : currentlyBroken) {
                    std::vector<Rule> nextRules = currentlyActive;
                    nextRules.erase(nextRules.begin() + i);
                    nextRules.push_back(r);
                    
                    if(CheckInventory(nextRules)) {
                        TryPushState(nextRules, "\n -> Break " + GetElementName(currentlyActive[i].noun) + " IS " + GetElementName(currentlyActive[i].prop) + 
                                                ", Reform " + GetElementName(r.noun) + " IS " + GetElementName(r.prop));
                    }
                }
            }

            // STRATEGY 6: Neutralize SINK (Push Object into Sink)
            // 1. Find reachable PUSH objects (Ammo)
            std::vector<std::pair<int,int>> ammo;
            for(int y=0; y<currentHeight; y++) {
                for(int x=0; x<currentWidth; x++) {
                    if(accessible[y*currentWidth+x]) {
                        const Cell& c = GetCell(s, x, y);
                        for(const auto& o : c.objects) {
                            if(HasProp(s, o.element, P_PUSH)) {
                                ammo.push_back({x, y});
                            }
                        }
                    }
                }
            }

            // 2. Find SINK objects adjacent to Reachable area (Targets)
            if (!ammo.empty()) {
                int dx[] = {1, -1, 0, 0}; int dy[] = {0, 0, 1, -1};
                for(int y=0; y<currentHeight; y++) {
                    for(int x=0; x<currentWidth; x++) {
                        const Cell& c = GetCell(s, x, y);
                        bool isSink = false;
                        int sinkElem = -1;
                        for(const auto& o : c.objects) {
                            if(HasProp(s, o.element, P_SINK)) {
                                isSink = true;
                                sinkElem = o.element;
                                break;
                            }
                        }
                        
                        if(isSink) {
                            // Check if adjacent to reach (i.e., we can push something INTO it)
                            bool adj = false;
                            for(int i=0; i<4; i++) {
                                int nx = x+dx[i], ny = y+dy[i];
                                if(nx>=0 && nx<currentWidth && ny>=0 && ny<currentHeight && reachable[ny*currentWidth+nx]) {
                                    adj = true; break;
                                }
                            }
                            
                            if(adj) {
                                GameState nextState = s;
                                // Remove Sink Object
                                Cell& sinkCell = GetCell(nextState, x, y);
                                for(auto it=sinkCell.objects.begin(); it!=sinkCell.objects.end(); ) { 
                                    if(it->element == sinkElem) { it=sinkCell.objects.erase(it); break; } else ++it; 
                                }
                                
                                // Remove Ammo (Take the first one)
                                std::pair<int,int> a = ammo[0];
                                Cell& ammoCell = GetCell(nextState, a.first, a.second);
                                for(auto it=ammoCell.objects.begin(); it!=ammoCell.objects.end(); ) { 
                                    if(HasProp(nextState, it->element, P_PUSH)) { it=ammoCell.objects.erase(it); break; } else ++it; 
                                }
                                
                                std::string h = GetLogicHash(nextState);
                                if(visited.find(h) == visited.end()) {
                                    visited.insert(h);
                                    q.push({nextState, current.plan + "\n -> Push Object into " + GetElementName(sinkElem)});
                                    std::cout << "  [Logic] Push Object into " << GetElementName(sinkElem) << std::endl;
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