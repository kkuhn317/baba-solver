#include "definitions.h"
#include "game.h"

std::string GetElementName(int e) {
    switch(e) {
        case WALL: return "WALL";
        case BABA: return "BABA";
        case FLAG: return "FLAG";
        case ROCK: return "ROCK";
        case WATER: return "WATER";
        case SKULL: return "SKULL";
        case TEXT_BABA: return "BABA";
        case TEXT_FLAG: return "FLAG";
        case TEXT_WALL: return "WALL";
        case TEXT_ROCK: return "ROCK";
        case TEXT_WATER: return "WATER";
        case TEXT_SKULL: return "SKULL";
        case TEXT_IS:   return "IS";
        case TEXT_YOU:  return "YOU";
        case TEXT_WIN:  return "WIN";
        case TEXT_STOP: return "STOP";
        case TEXT_PUSH: return "PUSH";
        case TEXT_SINK: return "SINK";
        case TEXT_DEFEAT: return "DEFEAT";
        default: return std::to_string(e);
    }
}

int CharToElement(char c) {
    switch(c) {
        case '#': return WALL;
        case 'B': return BABA;
        case 'F': return FLAG;
        case 'R': return ROCK;
        case 'W': return WATER;
        case 'S': return SKULL;
        case 'b': return TEXT_BABA;
        case 'f': return TEXT_FLAG;
        case 'w': return TEXT_WALL;
        case 'r': return TEXT_ROCK;
        case 'a': return TEXT_WATER;
        case 'u': return TEXT_SKULL;
        case 'i': return TEXT_IS;
        case 'y': return TEXT_YOU;
        case 'n': return TEXT_WIN;
        case 's': return TEXT_STOP;
        case 'p': return TEXT_PUSH;
        case 'k': return TEXT_SINK;
        case 'd': return TEXT_DEFEAT;
        default: return EMPTY;
    }
}

bool IsNoun(int e) { return (e >= TEXT_BABA && e <= TEXT_SKULL); }
bool IsProperty(int e) { return (e >= TEXT_YOU && e <= TEXT_DEFEAT); }

int TextToElement(int textID) {
    if (textID == TEXT_BABA) return BABA;
    if (textID == TEXT_ROCK) return ROCK;
    if (textID == TEXT_FLAG) return FLAG;
    if (textID == TEXT_WALL) return WALL;
    if (textID == TEXT_WATER) return WATER;
    if (textID == TEXT_SKULL) return SKULL;
    return 0;
}

PropFlags TextToProp(int textID) {
    if (textID == TEXT_YOU) return P_YOU;
    if (textID == TEXT_PUSH) return P_PUSH;
    if (textID == TEXT_STOP) return P_STOP;
    if (textID == TEXT_WIN) return P_WIN;
    if (textID == TEXT_SINK) return P_SINK;
    if (textID == TEXT_DEFEAT) return P_DEFEAT;
    return P_NONE;
}

char ElementToChar(int e) {
    switch(e) {
        case WALL: return '#';
        case BABA: return 'B';
        case FLAG: return 'F';
        case ROCK: return 'R';
        case WATER: return 'W';
        case SKULL: return 'S';
        case TEXT_BABA: return 'b';
        case TEXT_FLAG: return 'f';
        case TEXT_WALL: return 'w';
        case TEXT_ROCK: return 'r';
        case TEXT_WATER: return 'a';
        case TEXT_SKULL: return 'u';
        case TEXT_IS: return 'i';
        case TEXT_YOU: return 'y';
        case TEXT_WIN: return 'n';
        case TEXT_STOP: return 's';
        case TEXT_PUSH: return 'p';
        case TEXT_SINK: return 'k';
        case TEXT_DEFEAT: return 'd';
        default: return '.';
    }
}