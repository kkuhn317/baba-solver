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
#include <chrono>
#include <limits>
#include <memory>
#include <mutex>

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
std::string Solve(const GameState& startState, const std::atomic<bool>* cancel) {
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
        if (cancel && cancel->load()) return "Solver cancelled";
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

struct RuleGoalLayout {
    int cells[3];
};

struct RuleSearchContext {
    int noun = -1;
    int prop = -1;
    std::vector<bool> blocked;
    std::vector<RuleGoalLayout> layouts;
    std::map<int, std::vector<int>> reverseDistances;
    int constrainedElement = -1;
};

static std::vector<int> BuildReversePushDistances(const std::vector<bool>& blocked, int target) {
    const int unreachable = 1000000;
    std::vector<int> distance(currentWidth * currentHeight, unreachable);
    if (target < 0 || target >= static_cast<int>(distance.size()) || blocked[target]) return distance;

    std::queue<int> pending;
    distance[target] = 0;
    pending.push(target);
    const int dx[] = {1, -1, 0, 0};
    const int dy[] = {0, 0, 1, -1};

    while (!pending.empty()) {
        int cell = pending.front();
        pending.pop();
        int x = cell % currentWidth;
        int y = cell / currentWidth;
        for (int i = 0; i < 4; ++i) {
            int previousX = x - dx[i];
            int previousY = y - dy[i];
            int playerX = previousX - dx[i];
            int playerY = previousY - dy[i];
            if (previousX < 0 || previousX >= currentWidth || previousY < 0 || previousY >= currentHeight) continue;
            if (playerX < 0 || playerX >= currentWidth || playerY < 0 || playerY >= currentHeight) continue;
            int previous = previousY * currentWidth + previousX;
            int player = playerY * currentWidth + playerX;
            if (blocked[previous] || blocked[player]) continue;
            if (distance[previous] > distance[cell] + 1) {
                distance[previous] = distance[cell] + 1;
                pending.push(previous);
            }
        }
    }
    return distance;
}

static RuleSearchContext BuildRuleSearchContext(const GameState& state, int noun, int prop) {
    RuleSearchContext context;
    context.noun = noun;
    context.prop = prop;
    context.blocked.assign(currentWidth * currentHeight, false);

    for (int y = 0; y < currentHeight; ++y) {
        for (int x = 0; x < currentWidth; ++x) {
            const Cell& cell = GetCell(const_cast<GameState&>(state), x, y);
            for (const auto& object : cell.objects) {
                if (HasProp(const_cast<GameState&>(state), object.element, P_STOP) &&
                    !HasProp(const_cast<GameState&>(state), object.element, P_PUSH)) {
                    context.blocked[y * currentWidth + x] = true;
                    break;
                }
            }
        }
    }

    const int directions[2][2] = {{1, 0}, {0, 1}};
    for (const auto& direction : directions) {
        int dx = direction[0], dy = direction[1];
        for (int y = 0; y < currentHeight - 2 * dy; ++y) {
            for (int x = 0; x < currentWidth - 2 * dx; ++x) {
                RuleGoalLayout layout{{y * currentWidth + x,
                                       (y + dy) * currentWidth + x + dx,
                                       (y + 2 * dy) * currentWidth + x + 2 * dx}};
                if (context.blocked[layout.cells[0]] || context.blocked[layout.cells[1]] ||
                    context.blocked[layout.cells[2]]) continue;
                context.layouts.push_back(layout);
                for (int cell : layout.cells) {
                    if (context.reverseDistances.find(cell) == context.reverseDistances.end()) {
                        context.reverseDistances[cell] = BuildReversePushDistances(context.blocked, cell);
                    }
                }
            }
        }
    }

    const int roles[3] = {noun, TEXT_IS, prop};
    std::map<int, int> options;
    for (int role = 0; role < 3; ++role) options[roles[role]] = 0;
    for (const auto& layout : context.layouts) {
        for (int role = 0; role < 3; ++role) {
            const auto& distances = context.reverseDistances.at(layout.cells[role]);
            for (int y = 0; y < currentHeight; ++y) {
                for (int x = 0; x < currentWidth; ++x) {
                    const Cell& cell = GetCell(const_cast<GameState&>(state), x, y);
                    for (const auto& object : cell.objects) {
                        if (object.element == roles[role] && distances[y * currentWidth + x] < 1000000) {
                            options[roles[role]]++;
                        }
                    }
                }
            }
        }
    }
    int fewestOptions = std::numeric_limits<int>::max();
    for (const auto& option : options) {
        if (option.second > 0 && option.second < fewestOptions) {
            fewestOptions = option.second;
            context.constrainedElement = option.first;
        }
    }
    return context;
}

static int GetRulePushHeuristic(const GameState& state, const RuleSearchContext& context) {
    const int unreachable = 1000000;
    struct Occurrence { int element; int cell; int id; };
    std::vector<Occurrence> occurrences;
    int occurrenceId = 0;
    for (int y = 0; y < currentHeight; ++y) {
        for (int x = 0; x < currentWidth; ++x) {
            const Cell& cell = GetCell(const_cast<GameState&>(state), x, y);
            for (const auto& object : cell.objects) {
                if (object.element == context.noun || object.element == TEXT_IS || object.element == context.prop) {
                    occurrences.push_back({object.element, y * currentWidth + x, occurrenceId++});
                }
            }
        }
    }

    const int roles[3] = {context.noun, TEXT_IS, context.prop};
    int best = unreachable;
    for (const auto& layout : context.layouts) {
        std::function<void(int, int, std::set<int>&)> assign = [&](int role, int cost, std::set<int>& used) {
            if (cost >= best) return;
            if (role == 3) { best = cost; return; }
            const auto& distances = context.reverseDistances.at(layout.cells[role]);
            for (const auto& occurrence : occurrences) {
                if (occurrence.element != roles[role] || used.count(occurrence.id)) continue;
                int distance = distances[occurrence.cell];
                if (distance >= unreachable) continue;
                used.insert(occurrence.id);
                assign(role + 1, cost + distance, used);
                used.erase(occurrence.id);
            }
        };
        std::set<int> used;
        assign(0, 0, used);
    }
    return best >= unreachable ? 1000 : best;
}

static int GetHeuristic(const GameState& state, const RuleSearchContext* ruleContext) {
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

    if (ruleContext) {
        return GetRulePushHeuristic(state, *ruleContext);
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

struct PathLink {
    std::shared_ptr<const PathLink> parent;
    std::string segment;
};

static std::string BuildPath(const std::shared_ptr<const PathLink>& end) {
    std::vector<std::string> segments;
    size_t totalLength = 0;
    for (auto link = end; link; link = link->parent) {
        totalLength += link->segment.size();
        segments.push_back(link->segment);
    }
    std::string result;
    result.reserve(totalLength);
    for (auto it = segments.rbegin(); it != segments.rend(); ++it) result += *it;
    return result;
}

struct StateNode {
    GameState state;
    std::shared_ptr<const PathLink> path;
    int pushes;
    int heuristic; // A* Cost
    int focusPenalty;
    bool ruleSearch;

    // Priority Queue sorts primarily by Pushes (Cost), using Heuristic as tie-breaker
    bool operator>(const StateNode& other) const {
        if (ruleSearch) {
            int score = pushes + heuristic;
            int otherScore = other.pushes + other.heuristic;
            if (score != otherScore) return score > otherScore;
            if (focusPenalty != other.focusPenalty) return focusPenalty > other.focusPenalty;
        }
        // Prioritize Pushes FIRST, then Heuristic (Distance) as tie-breaker.
        if (pushes != other.pushes) return pushes > other.pushes;
        return heuristic > other.heuristic;
    }
};

static std::mutex ruleCacheMutex;
static std::unordered_map<std::string, std::string> ruleSolutionCache;
static std::unordered_set<std::string> failedRuleCache;

static std::string MakeRuleCacheKey(const GameState& source, int noun, int prop) {
    GameState state = source;
    return std::to_string(currentWidth) + "x" + std::to_string(currentHeight) + ":" +
           std::to_string(noun) + ":" + std::to_string(prop) + ":" + SerializeState(state);
}

static bool IsBoardCorner(int x, int y) {
    bool horizontalBoundary = (x == 0 || x == currentWidth - 1);
    bool verticalBoundary = (y == 0 || y == currentHeight - 1);
    return horizontalBoundary && verticalBoundary;
}

static bool IsGoalCellForElement(const RuleSearchContext& context, int element, int cell) {
    const int roles[3] = {context.noun, TEXT_IS, context.prop};
    for (const auto& layout : context.layouts) {
        for (int role = 0; role < 3; ++role) {
            if (roles[role] == element && layout.cells[role] == cell) return true;
        }
    }
    return false;
}

std::string SolveOptimized(const GameState& startState, int targetNoun, int targetProp,
                           int maxIterations, const std::atomic<bool>* cancel) {
    std::priority_queue<StateNode, std::vector<StateNode>, std::greater<StateNode>> pq;
    std::unordered_set<std::string> visited;
    bool solvingForRule = (targetNoun != -1);

    std::string cacheKey;
    if (solvingForRule) {
        cacheKey = MakeRuleCacheKey(startState, targetNoun, targetProp);
        std::lock_guard<std::mutex> lock(ruleCacheMutex);
        auto cached = ruleSolutionCache.find(cacheKey);
        if (cached != ruleSolutionCache.end()) {
            std::cout << "  [Push Search] Cache hit for " << GetElementName(targetNoun)
                      << " IS " << GetElementName(targetProp) << std::endl;
            return cached->second;
        }
        if (failedRuleCache.find(cacheKey) != failedRuleCache.end()) {
            std::cout << "  [Push Search] Cached impossible rule: " << GetElementName(targetNoun)
                      << " IS " << GetElementName(targetProp) << std::endl;
            return "";
        }
    }

    std::unique_ptr<RuleSearchContext> ruleContext;
    if (solvingForRule) {
        ruleContext = std::make_unique<RuleSearchContext>(
            BuildRuleSearchContext(startState, targetNoun, targetProp));
    }

    int h = GetHeuristic(startState, ruleContext.get());
    pq.push({startState, nullptr, 0, h, 0, solvingForRule});
    
    int dxs[] = {1, 0, -1, 0};
    int dys[] = {0, 1, 0, -1};
    char dcs[] = {'R', 'D', 'L', 'U'};
    
    int iterations = 0;
    int maxPushesLogged = -1;
    auto searchStarted = std::chrono::steady_clock::now();

    while(!pq.empty()) {
        if (cancel && cancel->load()) return "Solver cancelled";
        iterations++;
        if (iterations > maxIterations) break;

        StateNode current = pq.top(); pq.pop();
        GameState state = current.state;

        if (current.pushes > maxPushesLogged) {
            maxPushesLogged = current.pushes;
            std::cout << "Pushes: " << maxPushesLogged 
                      << " | Queue: " << pq.size() 
                      << " | Visited: " << visited.size() << std::endl;
        }

        if (iterations % 5000 == 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - searchStarted).count();
            std::string targetName = solvingForRule
                ? GetElementName(targetNoun) + " IS " + GetElementName(targetProp)
                : "WIN";
            std::cout << "  [Push Search] Target: " << targetName
                      << " | Processed: " << iterations
                      << " | Queue: " << pq.size()
                      << " | Unique: " << visited.size()
                      << " | Best h: " << current.heuristic
                      << " | Time: " << elapsed << "ms" << std::endl;
        }

        // CHECK GOAL
        if (solvingForRule) {
            if ((state.propertyMap[TextToElement(targetNoun)] & TextToProp(targetProp)) != 0) {
                std::string path = BuildPath(current.path);
                std::lock_guard<std::mutex> lock(ruleCacheMutex);
                ruleSolutionCache[cacheKey] = path;
                return path;
            }
        } else {
            if (state.hasWon) return BuildPath(current.path);
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
                                return BuildPath(current.path) + GetWalkPath(state, px, py, x, y);
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
                    bool pushedFocusedWord = false;
                    if (solvingForRule) {
                        bool deadlock = false;
                        int chainX = tx, chainY = ty;
                        while (chainX >= 0 && chainX < currentWidth && chainY >= 0 && chainY < currentHeight) {
                            const Cell& chainCell = GetCell(const_cast<GameState&>(state), chainX, chainY);
                            bool hasPush = false;
                            for (const auto& object : chainCell.objects) {
                                if (!HasProp(const_cast<GameState&>(state), object.element, P_PUSH)) continue;
                                hasPush = true;
                                if (object.element == ruleContext->constrainedElement) pushedFocusedWord = true;
                                int destinationX = chainX + dx;
                                int destinationY = chainY + dy;
                                if ((object.element == targetNoun || object.element == TEXT_IS || object.element == targetProp) &&
                                    IsBoardCorner(destinationX, destinationY) &&
                                    !IsGoalCellForElement(*ruleContext, object.element,
                                                          destinationY * currentWidth + destinationX)) {
                                    deadlock = true;
                                }
                            }
                            if (!hasPush || deadlock) break;
                            chainX += dx;
                            chainY += dy;
                        }
                        if (deadlock) continue;
                    }

                    GameState nextState = state;
                    TeleportPlayer(nextState, px, py, rx, ry);
                    nextState = MakeMove(nextState, dx, dy);
                    
                    if (FindPlayerPos(nextState).first == -1) continue;

                    // 2. Calculate New Heuristic for Child Node
                    int newH = GetHeuristic(nextState, ruleContext.get());
                    std::string walk = GetWalkPath(state, px, py, rx, ry);
                    if (walk.empty() && (px != rx || py != ry)) continue;

                    auto nextPath = std::make_shared<PathLink>(PathLink{current.path, walk + dcs[i]});
                    int focusPenalty = solvingForRule && !pushedFocusedWord ? 1 : 0;
                    pq.push({nextState, std::move(nextPath), current.pushes + 1, newH,
                             focusPenalty, solvingForRule});
                }
            }
        }
    }

    if (solvingForRule && pq.empty() && !(cancel && cancel->load())) {
        std::lock_guard<std::mutex> lock(ruleCacheMutex);
        failedRuleCache.insert(cacheKey);
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

struct ConcreteTransition {
    bool found = false;
    GameState state;
    std::string moves;
};

static bool IsRuleActive(const GameState& state, const Rule& rule) {
    if (IsProperty(rule.prop)) {
        return (state.propertyMap[TextToElement(rule.noun)] & TextToProp(rule.prop)) != 0;
    }
    if (IsNoun(rule.prop)) {
        int from = TextToElement(rule.noun);
        int to = TextToElement(rule.prop);
        for (const auto& transform : state.transformRules) {
            if (transform.first == from && transform.second == to) return true;
        }
    }
    return false;
}

// Search only through real game transitions. High-level logic is allowed to
// propose a goal, but it never gets to manufacture the successor state.
static ConcreteTransition FindConcreteTransition(
    const GameState& start,
    const std::function<bool(const GameState&)>& isGoal,
    int maxDepth,
    int maxStates,
    const std::function<bool(const GameState&, const GameState&)>& allowTransition = {},
    const std::atomic<bool>* cancel = nullptr) {
    struct Node {
        GameState state;
        std::string moves;
        int depth;
    };

    GameState initial = start;
    ParseRules(initial);
    CheckWin(initial);
    if (isGoal(initial)) return {true, initial, ""};

    std::queue<Node> search;
    std::unordered_set<std::string> seen;
    seen.insert(SerializeState(initial));
    search.push({initial, "", 0});

    const int dx[] = {1, 0, -1, 0};
    const int dy[] = {0, 1, 0, -1};
    const char dc[] = {'R', 'D', 'L', 'U'};

    while (!search.empty() && static_cast<int>(seen.size()) < maxStates) {
        if (cancel && cancel->load()) return {};
        Node current = std::move(search.front());
        search.pop();
        if (current.depth >= maxDepth) continue;

        for (int i = 0; i < 4; ++i) {
            GameState next = MakeMove(current.state, dx[i], dy[i]);
            if (allowTransition && !allowTransition(current.state, next)) continue;

            std::string hash = SerializeState(next);
            if (!seen.insert(hash).second) continue;

            std::string moves = current.moves + dc[i];
            if (isGoal(next)) return {true, std::move(next), std::move(moves)};
            search.push({std::move(next), std::move(moves), current.depth + 1});
        }
    }
    return {};
}

static std::string SerializeNonYouObjects(const GameState& source) {
    GameState state = source;
    for (auto& cell : state.grid) {
        cell.objects.erase(
            std::remove_if(cell.objects.begin(), cell.objects.end(), [&](const Object& object) {
                return HasProp(state, object.element, P_YOU);
            }),
            cell.objects.end());
    }
    return SerializeState(state);
}

static int CountElement(const GameState& state, int element) {
    int count = 0;
    for (const auto& cell : state.grid) {
        for (const auto& object : cell.objects) {
            if (object.element == element) ++count;
        }
    }
    return count;
}

static bool ReplayConcreteMoves(const GameState& start, const std::string& moves, GameState& result) {
    result = start;
    for (char move : moves) {
        int dx = 0;
        int dy = 0;
        if (move == 'R') dx = 1;
        else if (move == 'D') dy = 1;
        else if (move == 'L') dx = -1;
        else if (move == 'U') dy = -1;
        else return false;
        result = MakeMove(result, dx, dy);
    }
    return true;
}

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
    
    // Prefer goal/control rules before environmental properties and transformations.
    std::sort(potential.begin(), potential.end(), [](const Rule& a, const Rule& b) {
        auto priority = [](int p) {
            if (p == TEXT_WIN) return 0;
            if (p == TEXT_YOU) return 1;
            if (IsNoun(p)) return 2;
            return 3;
        };
        int aPriority = priority(a.prop);
        int bPriority = priority(b.prop);
        if (aPriority != bPriority) return aPriority < bPriority;
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
    q.push({initial, "", ""});
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

std::string LogicSolver::NextSolution(const std::atomic<bool>* cancel) {
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
        if (cancel && cancel->load()) return "Solver cancelled";
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

        // 1. Check Win using only replayable walking inputs. The non-YOU
        // signature guard prevents this shortcut from silently pushing text or
        // other objects, and also handles multiple YOU objects correctly.
        ConcreteTransition walkToWin = FindConcreteTransition(
            s,
            [](const GameState& candidate) { return candidate.hasWon; },
            currentWidth * currentHeight,
            20000,
            [](const GameState& before, const GameState& after) {
                return SerializeNonYouObjects(before) == SerializeNonYouObjects(after);
            }, cancel);
        if (walkToWin.found) {
            std::string sol = current.plan + "\n -> Reach WIN\nMOVES: " + current.moves + walkToWin.moves;
            std::vector<std::string> steps = ParsePlan(current.plan + "\n -> Reach WIN");
            if (!IsRedundant(steps)) {
                foundPlans.push_back(steps);
                return sol;
            }
            continue;
        }

        auto players = FindAllPlayerPos(s);
        if (!players.empty()) {
            auto reachable = GetReachableCells(s, players);

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

            // Identify Fixed vs Mutable Rules (And populate usedRuleCoords)
            std::vector<Rule> currentlyActive;
            struct ActiveRuleInfo {
                Rule rule;
                bool nounFixed;
                bool isFixed;
                bool propFixed;
            };
            std::vector<ActiveRuleInfo> activeRuleInfos;
            std::set<int> usedRuleCoords;

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
                    if (active) {
                        fixedRules.push_back({r.noun, r.prop});
                        usedRuleCoords.insert(r.y1 * currentWidth + r.x1);
                        usedRuleCoords.insert(r.y2 * currentWidth + r.x2);
                        usedRuleCoords.insert(r.y3 * currentWidth + r.x3);
                    }
                } else {
                    // Partially or Fully Movable
                    if (active) {
                        currentlyActive.push_back({r.noun, r.prop});
                        activeRuleInfos.push_back({{r.noun, r.prop}, !nP, !iP, !pP});
                        usedRuleCoords.insert(r.y1 * currentWidth + r.x1);
                        usedRuleCoords.insert(r.y2 * currentWidth + r.x2);
                        usedRuleCoords.insert(r.y3 * currentWidth + r.x3);
                    }
                }
            }

            // Populate Movable Inventory (Only Pushable text that is NOT used in an ACTIVE rule)
            std::map<int, std::vector<std::vector<bool>>> textReachability;
            for(int y=0; y<currentHeight; y++) {
                for(int x=0; x<currentWidth; x++) {
                    if (accessible[y*currentWidth+x]) {
                        const Cell& c = GetCell(s, x, y);
                        for(const auto& o : c.objects) {
                            if (o.element >= 10) {
                                if (IsPushable(x, y)) {
                                    if (usedRuleCoords.find(y*currentWidth+x) == usedRuleCoords.end()) {
                                        movableInventory[o.element]++;
                                        textReachability[o.element].push_back(GetPushableReach(s, x, y));
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (iterations == 1) {
                std::cout << "Initial Mutable Rules: " << currentlyActive.size() << std::endl;
                for(auto& r : currentlyActive) std::cout << " - " << GetSolverName(r.noun) << " IS " << GetSolverName(r.prop) << std::endl;
            }

            // Helper to apply rules and push state
            std::string immediateSolution;
            auto TryPushState = [&](const std::vector<Rule>& newMutableRules, 
                                    const std::string& stepDesc,
                                    const std::vector<std::tuple<int, int, int>>& placements,
                                    const std::string& lastBrokenRule = "") -> bool {
                if (cancel && cancel->load()) return false;
                auto goal = [&](const GameState& candidate) {
                    for (const auto& placement : placements) {
                        auto [element, x, y] = placement;
                        bool present = false;
                        const Cell& cell = GetCell(const_cast<GameState&>(candidate), x, y);
                        for (const auto& object : cell.objects) {
                            if (object.element == element) { present = true; break; }
                        }
                        if (!present) return false;
                    }

                    for (const auto& rule : newMutableRules) {
                        if (!IsRuleActive(candidate, rule)) return false;
                    }
                    for (const auto& oldRule : currentlyActive) {
                        bool retained = false;
                        for (const auto& newRule : newMutableRules) {
                            if (oldRule.noun == newRule.noun && oldRule.prop == newRule.prop) {
                                retained = true;
                                break;
                            }
                        }
                        if (!retained && IsRuleActive(candidate, oldRule)) return false;
                    }

                    return !FindAllPlayerPos(candidate).empty();
                };

                ConcreteTransition transition;

                // For an unconstrained property-rule goal, use the push-level
                // solver rather than guessing a physical three-cell location.
                // Replay its path from the real state before accepting it.
                if (placements.empty()) {
                    for (const auto& newRule : newMutableRules) {
                        bool wasActive = false;
                        for (const auto& oldRule : currentlyActive) {
                            if (oldRule.noun == newRule.noun && oldRule.prop == newRule.prop) {
                                wasActive = true;
                                break;
                            }
                        }
                        if (!wasActive && IsProperty(newRule.prop)) {
                            std::cout << "  [Logic] Searching for " << GetSolverName(newRule.noun)
                                      << " IS " << GetSolverName(newRule.prop) << std::endl;
                            std::string moves = SolveOptimized(s, newRule.noun, newRule.prop, 300000, cancel);
                            GameState replayed;
                            if (!moves.empty() && ReplayConcreteMoves(s, moves, replayed) && goal(replayed)) {
                                transition = {true, std::move(replayed), std::move(moves)};
                                break;
                            }
                        }
                    }
                }

                if (!transition.found) {
                    int maxDepth = (std::min)(40, 16 + static_cast<int>(placements.size()) * 6);
                    transition = FindConcreteTransition(s, goal, maxDepth, 50000, {}, cancel);
                }
                if (!transition.found || transition.moves.empty()) return false;

                std::string h = GetLogicHash(transition.state);
                if (visited.find(h) == visited.end()) {
                    visited.insert(h);

                    ConcreteTransition finish = FindConcreteTransition(
                        transition.state,
                        [](const GameState& candidate) { return candidate.hasWon; },
                        currentWidth * currentHeight,
                        20000,
                        [](const GameState& before, const GameState& after) {
                            return SerializeNonYouObjects(before) == SerializeNonYouObjects(after);
                        }, cancel);
                    if (finish.found) {
                        immediateSolution = current.plan + stepDesc + "\n -> Reach WIN\nMOVES: " +
                                            current.moves + transition.moves + finish.moves;
                        std::vector<std::string> steps = ParsePlan(current.plan + stepDesc + "\n -> Reach WIN");
                        if (!IsRedundant(steps)) foundPlans.push_back(steps);
                        return true;
                    }

                    q.push({transition.state, current.plan + stepDesc,
                            current.moves + transition.moves, lastBrokenRule});
                    std::cout << "  [Logic] " << stepDesc.substr(1) << std::endl; // substr(1) to skip newline
                }
                return false;
            };

            // Helper to check resources (Movable Inventory + Fixed Slots)
            auto CheckResources = [&](const std::vector<Rule>& targetRules) {
                std::map<int, int> inv = movableInventory;
                std::vector<ActiveRuleInfo> slots = activeRuleInfos;
                std::vector<bool> ruleSatisfied(targetRules.size(), false);
                
                // 1. Exact Matches (Maintenance)
                for(size_t i=0; i<targetRules.size(); i++) {
                    for(auto it = slots.begin(); it != slots.end(); ++it) {
                        if (it->rule.noun == targetRules[i].noun && it->rule.prop == targetRules[i].prop) {
                            slots.erase(it);
                            ruleSatisfied[i] = true;
                            break;
                        }
                    }
                }

                // 2. Partial Matches (Adaptation)
                for(size_t i=0; i<targetRules.size(); i++) {
                    if (ruleSatisfied[i]) continue;
                    const Rule& tr = targetRules[i];

                    for(auto it = slots.begin(); it != slots.end(); ++it) {
                        bool match = true;
                        if (it->nounFixed && it->rule.noun != tr.noun) match = false;
                        if (it->propFixed && it->rule.prop != tr.prop) match = false;
                        
                        if (match) {
                            int costNoun = (it->rule.noun == tr.noun) ? 0 : 1;
                            int costIs   = 0;
                            int costProp = (it->rule.prop == tr.prop) ? 0 : 1;
                            
                            if (inv[tr.noun] >= costNoun && inv[TEXT_IS] >= costIs && inv[tr.prop] >= costProp) {
                                inv[tr.noun] -= costNoun;
                                inv[TEXT_IS] -= costIs;
                                inv[tr.prop] -= costProp;
                                
                                // Recycle replaced components
                                if (costNoun == 1 && !it->nounFixed) inv[it->rule.noun]++;
                                if (costProp == 1 && !it->propFixed) inv[it->rule.prop]++;
                                
                                slots.erase(it);
                                ruleSatisfied[i] = true;
                                break;
                            }
                        }
                    }
                }

                // 3. Dismantle remaining slots
                for(const auto& s : slots) {
                    if (!s.nounFixed) inv[s.rule.noun]++;
                    if (!s.isFixed)   inv[TEXT_IS]++;
                    if (!s.propFixed) inv[s.rule.prop]++;
                }

                // 4. Build from Scratch
                for(size_t i=0; i<targetRules.size(); i++) {
                    if (ruleSatisfied[i]) continue;
                    const Rule& tr = targetRules[i];
                    
                    if (inv[tr.noun] >= 1 && inv[TEXT_IS] >= 1 && inv[tr.prop] >= 1) {
                        inv[tr.noun]--; inv[TEXT_IS]--; inv[tr.prop]--;
                        ruleSatisfied[i] = true;
                    }
                }
                
                for(bool b : ruleSatisfied) if(!b) return false;
                return true;
            };
            
            std::vector<Rule> potentialRules = GetPotentialRules(s); // All possible Noun-Prop pairs
            
            // STRATEGY 1: Break Active Rule
            for(size_t i=0; i<currentlyActive.size(); i++) {
                std::vector<Rule> nextRules = currentlyActive;
                nextRules.erase(nextRules.begin() + i);

                // Losing the final YOU rule is a terminal state, not a useful
                // logic branch. Retain the branch when some other fixed or
                // mutable YOU rule continues to provide control.
                if (currentlyActive[i].prop == TEXT_YOU) {
                    bool retainsYou = false;
                    for (const auto& rule : fixedRules) if (rule.prop == TEXT_YOU) retainsYou = true;
                    for (const auto& rule : nextRules) if (rule.prop == TEXT_YOU) retainsYou = true;
                    if (!retainsYou) continue;
                }
                if (TryPushState(nextRules, "\n -> Break " + GetSolverName(currentlyActive[i].noun) + " IS " + GetSolverName(currentlyActive[i].prop), {},
                                 currentlyActive[i].ToString())) {
                    return immediateSolution;
                }
                if (cancel && cancel->load()) return "Solver cancelled";
            }
            
            // STRATEGY 2: Form New Rule (from Inventory)
            for(const auto& p : potentialRules) {
                if (p.ToString() == current.lastBrokenRule) continue;
                // Check if p is already active
                bool active = false;
                for(const auto& ar : currentlyActive) if(ar.noun == p.noun && ar.prop == p.prop) active = true;
                if(active) continue;

                std::vector<Rule> nextRules = currentlyActive;
                nextRules.push_back(p);
                
                if(CheckResources(nextRules)) {
                    if (TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop), {})) {
                        return immediateSolution;
                    }
                    if (cancel && cancel->load()) return "Solver cancelled";
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
                        if (usedRuleCoords.find(y*currentWidth+x) != usedRuleCoords.end()) return false;
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
                                    if (TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Cross-Noun)",
                                        {{TEXT_IS, isX, isY}, {p.prop, prX, prY}})) return immediateSolution;
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
                                    if (TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Cross-Prop)",
                                        {{TEXT_IS, isX, isY}, {p.noun, nX, nY}})) return immediateSolution;
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
                                    if (TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Cross-IS)",
                                        {{p.noun, nX, nY}, {p.prop, prX, prY}})) return immediateSolution;
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
                                    if (TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Chain-Into-Noun)",
                                        {{TEXT_IS, isX, isY}, {p.noun, nX, nY}})) return immediateSolution;
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
                                    if (TryPushState(nextRules, "\n -> Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop) + " (Chain-From-Prop)",
                                        {{TEXT_IS, isX, isY}, {p.prop, prX, prY}})) return immediateSolution;
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
                         // Find location of currentlyActive[i] to swap
                         std::vector<std::tuple<int, int, int>> places;
                         for(const auto& r : currentLocs) {
                             if(r.noun == currentlyActive[i].noun && r.prop == currentlyActive[i].prop) {
                                 if(p.noun != r.noun) places.push_back({p.noun, r.x1, r.y1});
                                 if(p.prop != r.prop) places.push_back({p.prop, r.x3, r.y3});
                                 break;
                             }
                         }
                         if (TryPushState(nextRules, "\n -> Break " + GetSolverName(currentlyActive[i].noun) + " IS " + GetSolverName(currentlyActive[i].prop) + 
                                                 ", Form " + GetSolverName(p.noun) + " IS " + GetSolverName(p.prop), places)) return immediateSolution;
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
                            for(size_t k=0; k<ammoList.size(); k++) {
                                if (ammoList[k].x == x && ammoList[k].y == y) continue; // Don't push into self
                                if (isHazardTarget && !ammoList[k].isSink) continue; // Hazards require Sink Ammo
                                
                                if (ammoReach[k].empty()) ammoReach[k] = GetPushableReach(s, ammoList[k].x, ammoList[k].y);
                                
                                if (ammoReach[k][y * currentWidth + x]) {
                                    bestAmmoIdx = k;
                                    break;
                                }
                            }
                            
                            if(bestAmmoIdx != -1) {
                                const auto& a = ammoList[bestAmmoIdx];
                                int initialAmmoCount = CountElement(s, a.elem);
                                int initialTargetCount = CountElement(s, targetElem);
                                int ammoReduction = (a.elem == targetElem) ? 2 : 1;
                                auto neutralized = [&](const GameState& candidate) {
                                    if (a.elem == targetElem) {
                                        return CountElement(candidate, a.elem) <= initialAmmoCount - ammoReduction;
                                    }
                                    return CountElement(candidate, a.elem) < initialAmmoCount &&
                                           CountElement(candidate, targetElem) < initialTargetCount;
                                };
                                ConcreteTransition transition = FindConcreteTransition(
                                    s, neutralized, 40, 50000, {}, cancel);
                                if (!transition.found || transition.moves.empty()) continue;

                                std::string h = GetLogicHash(transition.state);
                                if(visited.find(h) == visited.end()) {
                                    visited.insert(h);
                                    std::string action = isSinkTarget ? " into " : " to neutralize ";
                                    q.push({transition.state,
                                            current.plan + "\n -> Push " + GetElementName(a.elem) + action + GetElementName(targetElem),
                                            current.moves + transition.moves});
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
                                auto pathIsOpen = [&](const GameState& candidate) {
                                    auto candidatePlayers = FindAllPlayerPos(candidate);
                                    if (candidatePlayers.empty()) return false;
                                    auto candidateReachable = GetReachableCells(candidate, candidatePlayers);
                                    return candidateReachable[targetY * currentWidth + targetX] != false;
                                };
                                ConcreteTransition transition = FindConcreteTransition(
                                    s, pathIsOpen, 30, 40000, {}, cancel);
                                if (!transition.found || transition.moves.empty()) continue;

                                std::string h = GetLogicHash(transition.state);
                                if (visited.find(h) == visited.end()) {
                                    visited.insert(h);
                                    q.push({transition.state,
                                            current.plan + "\n -> Push " + GetElementName(firstPushElem) + " to clear path",
                                            current.moves + transition.moves});
                                    std::cout << "  [Logic] Legal moves cleared path to ("
                                              << targetX << "," << targetY << ")" << std::endl;
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
