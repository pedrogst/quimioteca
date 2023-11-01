@echo off
gcc -oout/game.exe -Ilib/win-mingw -Ilib -Llib/win-mingw chemio.c -static -lraylib -lopengl32 -lgdi32 -lwinmm -O2 -Wall
