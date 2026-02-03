#ifndef LEVELS_H
#define LEVELS_H

#include <vector>
#include <string>
#include <map>

// --- LEVEL DATA ---
struct LevelDef {
    std::map<char, std::string> legend;
    std::vector<std::string> layout;
};

extern std::vector<LevelDef> levels;
void InitLevels();

#endif