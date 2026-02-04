#include "definitions.h"
#include <vector>
#include <map>

// Internal Colors
const COLORREF C_WALL = RGB(111, 118, 131);
const COLORREF C_BABA = RGB(255, 255, 255);
const COLORREF C_ROCK = RGB(168, 111, 80);
const COLORREF C_FLAG = RGB(247, 216, 97);
const COLORREF C_TEXT_PINK = RGB(255, 68, 153);
const COLORREF C_TEXT_WHITE = RGB(255, 255, 255);
const COLORREF C_WATER = RGB(0, 119, 211);
const COLORREF C_DEFEAT = RGB(139, 0, 0);
const COLORREF C_LAVA = RGB(255, 140, 0);
const COLORREF C_TEXT_GREEN = RGB(0, 200, 0);
const COLORREF C_TEXT_HOT = RGB(255, 140, 0);
const COLORREF C_TEXT_MELT = RGB(100, 200, 255);

static ElementDef registry[100];
static bool initialized = false;

void InitRegistry() {
    if(initialized) return;
    
    // Defaults
    for(int i=0; i<100; i++) registry[i] = { i, "UNKNOWN", "?", TYPE_NONE, '.', RGB(255,0,255), false, 0, P_NONE };

    // Objects
    registry[EMPTY] = { EMPTY, "EMPTY", " ", TYPE_NONE, '.', 0, false, 0, P_NONE };
    registry[WALL]  = { WALL, "WALL", "WALL", TYPE_OBJECT, '#', C_WALL, false, TEXT_WALL, P_NONE };
    registry[BABA]  = { BABA, "BABA", "BABA", TYPE_OBJECT, 'B', C_BABA, false, TEXT_BABA, P_NONE };
    registry[FLAG]  = { FLAG, "FLAG", "FLAG", TYPE_OBJECT, 'F', C_FLAG, false, TEXT_FLAG, P_NONE };
    registry[ROCK]  = { ROCK, "ROCK", "ROCK", TYPE_OBJECT, 'R', C_ROCK, false, TEXT_ROCK, P_NONE };
    registry[WATER] = { WATER, "WATER", "WATER", TYPE_OBJECT, 'W', C_WATER, false, TEXT_WATER, P_NONE };
    registry[SKULL] = { SKULL, "SKULL", "SKULL", TYPE_OBJECT, 'S', C_DEFEAT, false, TEXT_SKULL, P_NONE };
    registry[LAVA]  = { LAVA, "LAVA", "LAVA", TYPE_OBJECT, 'L', C_LAVA, false, TEXT_LAVA, P_NONE };

    // Text Nouns
    registry[TEXT_BABA]  = { TEXT_BABA, "TEXT_BABA", "BABA", TYPE_TEXT_NOUN, 'b', C_TEXT_PINK, false, BABA, P_NONE };
    registry[TEXT_FLAG]  = { TEXT_FLAG, "TEXT_FLAG", "FLAG", TYPE_TEXT_NOUN, 'f', C_FLAG, false, FLAG, P_NONE };
    registry[TEXT_WALL]  = { TEXT_WALL, "TEXT_WALL", "WALL", TYPE_TEXT_NOUN, 'w', C_WALL, false, WALL, P_NONE };
    registry[TEXT_ROCK]  = { TEXT_ROCK, "TEXT_ROCK", "ROCK", TYPE_TEXT_NOUN, 'r', C_ROCK, false, ROCK, P_NONE };
    registry[TEXT_WATER] = { TEXT_WATER, "TEXT_WATER", "WATER", TYPE_TEXT_NOUN, 'a', C_WATER, false, WATER, P_NONE };
    registry[TEXT_SKULL] = { TEXT_SKULL, "TEXT_SKULL", "SKULL", TYPE_TEXT_NOUN, 'u', C_DEFEAT, false, SKULL, P_NONE };
    registry[TEXT_LAVA]  = { TEXT_LAVA, "TEXT_LAVA", "LAVA", TYPE_TEXT_NOUN, 'l', C_LAVA, false, LAVA, P_NONE };

    // Text Operators
    registry[TEXT_IS] = { TEXT_IS, "IS", "IS", TYPE_TEXT_OP, 'i', C_TEXT_WHITE, false, 0, P_NONE };

    // Text Properties
    registry[TEXT_YOU]    = { TEXT_YOU, "YOU", "YOU", TYPE_TEXT_PROP, 'y', C_TEXT_PINK, true, 0, P_YOU };
    registry[TEXT_WIN]    = { TEXT_WIN, "WIN", "WIN", TYPE_TEXT_PROP, 'n', C_FLAG, true, 0, P_WIN };
    registry[TEXT_STOP]   = { TEXT_STOP, "STOP", "STOP", TYPE_TEXT_PROP, 's', C_TEXT_GREEN, true, 0, P_STOP };
    registry[TEXT_PUSH]   = { TEXT_PUSH, "PUSH", "PUSH", TYPE_TEXT_PROP, 'p', C_ROCK, true, 0, P_PUSH };
    registry[TEXT_SINK]   = { TEXT_SINK, "SINK", "SINK", TYPE_TEXT_PROP, 'k', C_WATER, true, 0, P_SINK };
    registry[TEXT_DEFEAT] = { TEXT_DEFEAT, "DEFEAT", "DEFEAT", TYPE_TEXT_PROP, 'd', C_DEFEAT, true, 0, P_DEFEAT };
    registry[TEXT_HOT]    = { TEXT_HOT, "HOT", "HOT", TYPE_TEXT_PROP, 'h', C_TEXT_HOT, true, 0, P_HOT };
    registry[TEXT_MELT]   = { TEXT_MELT, "MELT", "MELT", TYPE_TEXT_PROP, 'm', C_TEXT_MELT, true, 0, P_MELT };

    initialized = true;
}

const ElementDef& GetElementDef(int e) {
    if(!initialized) InitRegistry();
    if(e < 0 || e >= 100) return registry[0];
    return registry[e];
}

std::string GetElementName(int e) {
    return GetElementDef(e).name;
}

int ElementFromString(const std::string& name) {
    if(!initialized) InitRegistry();
    for(int i=0; i<100; i++) {
        if(registry[i].name == name) return i;
    }
    return EMPTY;
}

bool IsNoun(int e) { 
    return GetElementDef(e).type == TYPE_TEXT_NOUN; 
}

bool IsProperty(int e) { 
    return GetElementDef(e).type == TYPE_TEXT_PROP; 
}

int TextToElement(int textID) {
    const ElementDef& def = GetElementDef(textID);
    if (def.type == TYPE_TEXT_NOUN) return def.associatedId;
    return 0; 
}

PropFlags TextToProp(int textID) {
    const ElementDef& def = GetElementDef(textID);
    if (def.type == TYPE_TEXT_PROP) return def.propFlag;
    return P_NONE; 
}

char GetDefaultChar(int e) {
    return GetElementDef(e).symbol;
}