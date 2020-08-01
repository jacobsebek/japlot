#pragma once

typedef struct pointf pointf;
typedef struct pointi pointi;
#include "objects.h" // set
#include "SDL_gpu.h" // GPU_Target

typedef struct pointf {
    double x, y;
} pointf;

typedef struct pointi {
    int x, y;
} pointi;

int pointf_compare(const pointf* p1, const pointf* p2);

void graph(double start, double end, unsigned numsteps, set_s* dst);
void plot(GPU_Target* target, set_s* s);
