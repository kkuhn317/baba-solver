#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <string>

// --- ENTITY DEFINITIONS ---
enum Element {
    EMPTY = 0, WALL = 1, BABA = 2, ROCK = 3, FLAG = 4, WATER = 5, SKULL = 6,
    // Text IDs start at 10 to separate them from objects
    TEXT_BABA = 10, TEXT_ROCK = 11, TEXT_FLAG = 12, TEXT_WALL = 13, TEXT_WATER = 14, TEXT_SKULL = 15,
    TEXT_IS = 20, TEXT_YOU = 21, TEXT_PUSH = 22, TEXT_WIN = 23, TEXT_STOP = 24, TEXT_SINK = 25
};

// Properties bitmask
enum PropFlags { P_NONE = 0, P_YOU = 1, P_PUSH = 2, P_STOP = 4, P_WIN = 8, P_SINK = 16, P_DEFEAT = 32};

// --- HELPER FUNCTIONS ---
std::string GetElementName(int e);
int ElementFromString(const std::string& name);
bool IsNoun(int e);
bool IsProperty(int e);
int TextToElement(int textID);
PropFlags TextToProp(int textID);
char GetDefaultChar(int e);

#endif