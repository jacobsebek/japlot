#pragma once
#include "SDL_gpu.h"

typedef struct font_s {
	GPU_Image* img;
	unsigned rows, cols;
} font_s;

const font_s font_load(const char* filename, unsigned rows, unsigned columns, SDL_Color color);
void font_destroy(font_s font);
//void font_draw_char(const font_s font, char c);
void font_draw_string(GPU_Target* target, int x, int y, float scale, const font_s font, const char* str, const unsigned (*cindex)(const char));
