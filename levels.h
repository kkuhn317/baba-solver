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

const std::vector<LevelDef> levels = {
    // LEVEL 1: Where Do I Go?
    {
        { {'#',"WALL"}, {'B',"BABA"}, {'F',"FLAG"}, {'b',"TEXT_BABA"}, {'w',"TEXT_WALL"}, {'i',"IS"}, {'y',"YOU"}, {'n',"WIN"}, {'f',"TEXT_FLAG"}, {'s',"STOP"} },
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
        }
    },

    // LEVEL 2: Out of Reach
    {
        { {'#',"WALL"}, {'B',"BABA"}, {'R',"ROCK"}, {'W',"WATER"}, {'b',"TEXT_BABA"}, {'w',"TEXT_WALL"}, {'a',"TEXT_WATER"}, {'i',"IS"}, {'y',"YOU"}, {'s',"STOP"}, {'k',"SINK"}, {'r',"TEXT_ROCK"}, {'p',"PUSH"}, {'n',"WIN"}, {'f',"TEXT_FLAG"}, {'F',"FLAG"} },
        {
        "...########...",
        "...#......#...",
        "...#.B..R.#...",
        "bwa#......#...",
        "iii#....R.#...",
        "ysk#......#...",
        "####WWW#######",
        "#......#.....#",
        "#......#.rip.#",
        "#......#.....#",
        "#WWW.#.......#",
        "#WWW...#.fin.#",
        "#FWW...#.....#",
        "##############"
        }
    },

    // LEVEL 3: Off Limits
    {
        { {'#',"WALL"}, {'B',"BABA"}, {'R',"ROCK"}, {'W',"WATER"}, {'F',"FLAG"}, {'r',"TEXT_ROCK"}, {'i',"IS"}, {'s',"STOP"}, {'a',"TEXT_WATER"}, {'d',"DEFEAT"}, {'f',"TEXT_FLAG"}, {'n',"WIN"}, {'w',"TEXT_WALL"}, {'b',"TEXT_BABA"}, {'y',"YOU"} },
        {
        "ris.R.......W...........",
        "....R.....##W#######....",
        "aid.R.....#.W..#...#....",
        "....R.....#.W....F.#....",
        "fin.R.#####.W..#...#....",
        "....R.#...#.W..#####....",
        "RRRRR.#.B...W###........",
        "......#...#.WWWWWWWWWWWW",
        "......#####....#........",
        "..........#.wis#........",
        "..........#....#........",
        "........b.#....#........",
        "........i.######........",
        "........y...............",
        "........................."
        }
    },

    // LEVEL 4: Sinking Feeling
    {
        { {'#',"WALL"}, {'B',"BABA"}, {'R',"ROCK"}, {'W',"WATER"}, {'F',"FLAG"}, {'b',"TEXT_BABA"}, {'i',"IS"}, {'y',"YOU"}, {'w',"TEXT_WALL"}, {'s',"STOP"}, {'n',"WIN"}, {'r',"TEXT_ROCK"}, {'p',"PUSH"}, {'a',"TEXT_WATER"}, {'k',"SINK"}, {'f',"TEXT_FLAG"} },
        {
        "####################",
        "#biywis....#...fin.#",
        "#..........#.......#",
        "#..B.......#...F...#",
        "#..........#.......#",
        "#..R.......W.......#",
        "#..R.......W.......#",
        "#..R.......W.......#",
        "#..........W.......#",
        "#rip.......W...aik.#",
        "####################"
        }
    },

    // LEVEL 4: Sinking Feeling Edit
    {
        { {'#',"WALL"}, {'B',"BABA"}, {'R',"ROCK"}, {'W',"WATER"}, {'F',"FLAG"}, {'b',"TEXT_BABA"}, {'i',"IS"}, {'y',"YOU"}, {'w',"TEXT_WALL"}, {'s',"STOP"}, {'n',"WIN"}, {'r',"TEXT_ROCK"}, {'p',"PUSH"}, {'a',"TEXT_WATER"}, {'k',"SINK"}, {'f',"TEXT_FLAG"} },
        {
        "####################",
        "#biywis#...#...fin.#",
        "#######....#.......#",
        "#..B.......#...F...#",
        "#..........#.......#",
        "#..R.......W.......#",
        "#..R.......W.......#",
        "#..R.......W.......#",
        "#.r.ip.....W.......#",
        "#..........W...aik.#",
        "####################"
        }
    }
};

#endif