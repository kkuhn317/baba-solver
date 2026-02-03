#ifndef LEVELS_H
#define LEVELS_H

#include <vector>
#include <string>
#include <map>

// --- SIMPLE LEVEL MAKER GUIDE ---
// Use these characters in the 'levels' array below to design your levels:
//
// OBJECTS:
// . = Empty      # = Wall       B = Baba       F = Flag       R = Rock       W = Water
//
// TEXT (RULES):
// b = Text BABA  f = Text FLAG  w = Text WALL  r = Text ROCK  a = Text WATER
// i = Text IS    y = Text YOU   n = Text WIN   s = Text STOP  p = Text PUSH  k = Text SINK d = Text DEFEAT

// --- LEVEL DATA ---
struct LevelDef {
    std::map<char, std::string> legend;
    std::vector<std::string> layout;
};

extern std::vector<LevelDef> levels;
void InitLevels();

#endif