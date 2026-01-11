#ifndef LEVELS_H
#define LEVELS_H

#include <vector>
#include <string>

// --- SIMPLE LEVEL MAKER GUIDE ---
// Use these characters in the 'levels' array below to design your levels:
//
// OBJECTS:
// . = Empty      # = Wall       B = Baba       F = Flag       R = Rock
//
// TEXT (RULES):
// b = Text BABA  f = Text FLAG  w = Text WALL  r = Text ROCK
// i = Text IS    y = Text YOU   n = Text WIN   s = Text STOP  p = Text PUSH

// --- LEVEL DATA ---
const std::vector<std::vector<std::string>> levels = {
    // LEVEL 1: Introduction
    {
        "....########",
        "....#......#",
        "....#.i....#",
        "....#......#",
        "#####....n.#",
        "#..........#",
        "#.f...F....#",
        "#..........#",
        "#..........#",
        "############",
        "....#......#",
        ".b..#.w....#",
        ".i..#.i..B.#",
        ".y..#.s....#",
        "....#......#",
        "....########",
    },

    // LEVEL 2: Pushing Rocks
    {
        "....................",
        "......biy...........",
        "....................",
        "......B...R...F.....",
        "....................",
        "......rip...fin.....",
        "....................",
        "....................",
        "....................",
        "....................",
        "....................",
        "...................."
    },

    // LEVEL 3: Wall Breaker (The text is the key!)
    {
        "....................",
        "....................",
        ".......#.#..........",
        "..biy..#.#..fin.....",
        ".......#.#..........",
        ".......B.#...F......",
        ".......#.#..........",
        ".......#.#..........",
        ".......#.#..........",
        "..wis..#.#..........",
        ".......p............",
        "...................."
    }
};

#endif