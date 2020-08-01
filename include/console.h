#pragma once

#include "SDL.h"

#ifdef _WIN32
	#define CLEAR "cls"
#else
	#define CLEAR "clear"
#endif

#define COMMAND_MAXLEN 128LU

#define ANSI_COLOR_RED     "\x1b[91m"
#define ANSI_COLOR_GREEN   "\x1b[92m"
#define ANSI_COLOR_YELLOW  "\x1b[93m"
#define ANSI_COLOR_DYELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[94m"
#define ANSI_COLOR_MAGENTA "\x1b[95m"
#define ANSI_COLOR_CYAN    "\x1b[96m"
#define ANSI_COLOR_RESET   "\x1b[0m"

typedef struct {
    SDL_Color col_background;
    SDL_Color col_grid;
    SDL_Color col_text;

    double grid_size;
    double cam_movespeed;
    double cam_scalespeed;

    unsigned WIDTH, HEIGHT;
} settings_s;

extern settings_s settings;

void* console_start(void*);
void console_cleanup();
