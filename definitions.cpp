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
        case TEXT_BABA: return "TEXT_BABA";
        case TEXT_FLAG: return "TEXT_FLAG";
        case TEXT_WALL: return "TEXT_WALL";
        case TEXT_ROCK: return "TEXT_ROCK";
        case TEXT_WATER: return "TEXT_WATER";
        case TEXT_SKULL: return "TEXT_SKULL";
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

int ElementFromString(const std::string& name) {
    if (name == "WALL") return WALL;
    if (name == "BABA") return BABA;
    if (name == "FLAG") return FLAG;
    if (name == "ROCK") return ROCK;
    if (name == "WATER") return WATER;
    if (name == "SKULL") return SKULL;
    if (name == "TEXT_BABA") return TEXT_BABA;
    if (name == "TEXT_FLAG") return TEXT_FLAG;
    if (name == "TEXT_WALL") return TEXT_WALL;
    if (name == "TEXT_ROCK") return TEXT_ROCK;
    if (name == "TEXT_WATER") return TEXT_WATER;
    if (name == "TEXT_SKULL") return TEXT_SKULL;
    if (name == "IS") return TEXT_IS;
    if (name == "YOU") return TEXT_YOU;
    if (name == "WIN") return TEXT_WIN;
    if (name == "STOP") return TEXT_STOP;
    if (name == "PUSH") return TEXT_PUSH;
    if (name == "SINK") return TEXT_SINK;
    if (name == "DEFEAT") return TEXT_DEFEAT;
    return EMPTY;
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

char GetDefaultChar(int e) {
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