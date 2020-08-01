#pragma once

//#include "plot.h" // points
#include <pthread.h> // mutex

#define WORLD2CAM(p) (((pointi){(p.x-cam.x)/(cam.w/(double)settings.WIDTH), (p.y-cam.y)/(cam.h/(double)settings.HEIGHT)}))
#define WORLD2CAMCART(p) (((pointi){(p.x-cam.x)/(cam.w/(double)settings.WIDTH), (-p.y-cam.y)/(cam.h/(double)settings.HEIGHT)})) // CARThesian
#define CAM2WORLD(p) (((pointf){(p.x*(cam.w/(double)settings.WIDTH)+cam.x), p.y*(cam.h/(double)settings.HEIGHT)+cam.y})

#define COLDARKER1(col) ((SDL_Color){col.r*0.8, col.g*0.8, col.b*0.8, col.a})
#define COLDARKER2(col) ((SDL_Color){col.r*0.5, col.g*0.5, col.b*0.5, col.a})
#define COL2INT(col) (*(uint32_t*)&col)
#define COL2ARGS(col) col.r, col.g, col.b, col.a

typedef struct rectf { double x,y,w,h; } rectf;
extern rectf cam;
extern pthread_mutex_t renderer_mutex;

int window_init();
int window_destroy();

int window_update();
int window_draw();

