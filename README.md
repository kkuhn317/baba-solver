to build: open x64 Native Tools and type this

cl /std:c++17 main.cpp game.cpp movement.cpp solver.cpp definitions.cpp user32.lib gdi32.lib /EHsc /link /SUBSYSTEM:WINDOWS

to build and run the solver regression tests with MinGW g++:

g++ -std=c++17 -O2 -I. tests/solver_tests.cpp game.cpp movement.cpp solver.cpp definitions.cpp -luser32 -lgdi32 -o solver_tests.exe
.\solver_tests.exe
