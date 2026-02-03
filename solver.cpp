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

std::string SolveLogic(const GameState& startState, const std::unordered_set<std::string>& forbidden) {
    struct LogicNode { 
        GameState state; 
        std::string plan; 
        bool padding = false;
    };
    
    std::queue<LogicNode> q;
    std::unordered_set<std::string> visited;

    // Initial State
    GameState initial = startState;
    ParseRules(initial); // Ensure propertyMap is populated
    q.push({initial, ""});
    visited.insert(GetLogicHash(initial));
    
    auto GetSolverName = [](int e) {
        std::string name = GetElementName(e);
        if (name.length() > 5 && name.substr(0, 5) == "TEXT_") {
            return name.substr(5);
        }
        return name;
    };

    auto GetSteps = [](const std::string& plan) {
        std::vector<std::string> steps;
        size_t pos = 0;
        while ((pos = plan.find("->", pos)) != std::string::npos) {
            pos += 2;
            size_t end = plan.find("->", pos);
            std::string step = plan.substr(pos, end - pos);
            size_t first = step.find_first_not_of(" \n\r\t");
            if (first != std::string::npos) {
                size_t last = step.find_last_not_of(" \n\r\t");
                steps.push_back(step.substr(first, last - first + 1));
            }
            if (end == std::string::npos) break;
        }
        return steps;
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
                                    
                                    bool isRedundant = false;
                                    if (forbidden.find(sol) != forbidden.end()) {
                                        isRedundant = true;
                                    } else {
                                        std::vector<std::string> currSteps = GetSteps(sol);
                                        for (const auto& bad : forbidden) {
                                            std::vector<std::string> badSteps = GetSteps(bad);
                                            if (badSteps.size() > currSteps.size()) continue;
                                            
                                            size_t i = 0, j = 0;
                                            while (i < badSteps.size() && j < currSteps.size()) {
                                                if (badSteps[i] == currSteps[j]) i++;
                                                j++;
                                            }
                                            if (i == badSteps.size()) { isRedundant = true; break; }
                                        }
                                    }
                                    if (!isRedundant) return sol;
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
            for(int y=0; y<currentHeight; y++) {
                for(int x=0; x<currentWidth; x++) {
                    if (accessible[y*currentWidth+x]) {
                        const Cell& c = GetCell(s, x, y);
                        for(const auto& o : c.objects) {
                            if (o.element >= 10) {
                                if (IsPushable(x, y)) {
                                    if (criticalCoords.find(y*currentWidth+x) == criticalCoords.end()) {
                                        movableInventory[o.element]++;
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
                    int p_dx = r_dy; // Perpendicular
                    int p_dy = r_dx;
                    
                    auto IsFree = [&](int x, int y) {
                        if (x < 0 || x >= currentWidth || y < 0 || y >= currentHeight) return false;
                        if (!accessible[y*currentWidth+x]) return false;
                        const Cell& c = GetCell(const_cast<GameState&>(s), x, y);
                        for(const auto& o : c.objects) {
                            if (HasProp(const_cast<GameState&>(s), o.element, P_STOP) && !HasProp(const_cast<GameState&>(s), o.element, P_PUSH)) return false;
                        }
                        return true;
                    };

                    // 1. Share NOUN
                    if (r.noun == p.noun) {
                        // Check for space perpendicular to Noun (r.x1, r.y1)
                        // Need space for IS and PROP
                        bool fwd = IsFree(r.x1 + p_dx, r.y1 + p_dy) && IsFree(r.x1 + 2*p_dx, r.y1 + 2*p_dy);
                        bool bwd = IsFree(r.x1 - p_dx, r.y1 - p_dy) && IsFree(r.x1 - 2*p_dx, r.y1 - 2*p_dy);
                        
                        if ((fwd || bwd) && movableInventory[TEXT_IS] >= 1 && movableInventory[p.prop] >= 1) {
                            std::vector<Rule> nextRules = currentlyActive;
                            nextRules.push_back(p);
                            TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Cross-Noun)");
                        }
                    }

                    // 2. Share PROP
                    if (r.prop == p.prop) {
                        // Check for space perpendicular to Prop (r.x3, r.y3)
                        // Need space for NOUN and IS
                        // Note: If we build UP from Prop, it's Prop <- IS <- Noun.
                        // This means Noun is at (y-2), IS at (y-1), Prop at (y).
                        // This matches the "Backward" check relative to Prop if we consider flow.
                        // But simpler: just check if 2 spots are free in either direction.
                        // If we have free spots, we can put Noun and IS there.
                        // Since we are forming Noun IS Prop, and Prop is fixed:
                        // If we go (0, -1) (Up): (x, y-1) is IS, (x, y-2) is Noun.
                        // If we go (0, 1) (Down): (x, y+1) is IS, (x, y+2) is Noun.
                        
                        bool fwd = IsFree(r.x3 + p_dx, r.y3 + p_dy) && IsFree(r.x3 + 2*p_dx, r.y3 + 2*p_dy);
                        bool bwd = IsFree(r.x3 - p_dx, r.y3 - p_dy) && IsFree(r.x3 - 2*p_dx, r.y3 - 2*p_dy);

                        if ((fwd || bwd) && movableInventory[p.noun] >= 1 && movableInventory[TEXT_IS] >= 1) {
                            std::vector<Rule> nextRules = currentlyActive;
                            nextRules.push_back(p);
                            TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Cross-Prop)");
                        }
                    }

                    // 3. Share IS
                    // IS is at (r.x2, r.y2)
                    // Need space for NOUN and PROP on opposite sides
                    bool possible = IsFree(r.x2 - p_dx, r.y2 - p_dy) && IsFree(r.x2 + p_dx, r.y2 + p_dy);
                    
                    if (possible && movableInventory[p.noun] >= 1 && movableInventory[p.prop] >= 1) {
                        std::vector<Rule> nextRules = currentlyActive;
                        nextRules.push_back(p);
                        TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Cross-IS)");
                    }

                    // 4. Share Noun as Prop (Target Prop == Existing Noun)
                    if (r.noun == p.prop) {
                        // We want p.noun IS p.prop (where p.prop is r.noun).
                        // Need p.noun IS -> r.noun. Space "before" r.noun.
                        bool bwd = IsFree(r.x1 - p_dx, r.y1 - p_dy) && IsFree(r.x1 - 2*p_dx, r.y1 - 2*p_dy);
                        
                        if (bwd && movableInventory[p.noun] >= 1 && movableInventory[TEXT_IS] >= 1) {
                            std::vector<Rule> nextRules = currentlyActive;
                            nextRules.push_back(p);
                            TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Cross-Transform-1)");
                        }
                    }

                    // 5. Share Prop as Noun (Target Noun == Existing Prop)
                    if (r.prop == p.noun) {
                        // We want p.noun IS p.prop (where p.noun is r.prop).
                        // Need r.prop -> IS p.prop. Space "after" r.prop.
                        bool fwd = IsFree(r.x3 + p_dx, r.y3 + p_dy) && IsFree(r.x3 + 2*p_dx, r.y3 + 2*p_dy);
                        
                        if (fwd && movableInventory[TEXT_IS] >= 1 && movableInventory[p.prop] >= 1) {
                            std::vector<Rule> nextRules = currentlyActive;
                            nextRules.push_back(p);
                            TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Cross-Transform-2)");
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

            // STRATEGY 5: Neutralize SINK (Push Object into Sink)
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
                                int pushedElem = -1;
                                for(auto it=ammoCell.objects.begin(); it!=ammoCell.objects.end(); ) { 
                                    if(HasProp(nextState, it->element, P_PUSH)) { pushedElem = it->element; it=ammoCell.objects.erase(it); break; } else ++it; 
                                }
                                
                                std::string h = GetLogicHash(nextState);
                                if(visited.find(h) == visited.end()) {
                                    visited.insert(h);
                                    q.push({nextState, current.plan + "\n -> Push " + GetElementName(pushedElem) + " into " + GetElementName(sinkElem)});
                                    std::cout << "  [Logic] Push " << GetElementName(pushedElem) << " into " << GetElementName(sinkElem) << std::endl;
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