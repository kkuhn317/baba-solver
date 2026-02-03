to build: open x64 Native Tools and type this

cl /std:c++17 main.cpp game.cpp movement.cpp solver.cpp definitions.cpp user32.lib gdi32.lib /EHsc /link /SUBSYSTEM:WINDOWS