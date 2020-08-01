#include "renderer.h"

#include "SDL_gpu.h"
#include "SDL.h"
#include "font.h"

#include "error.h"

#include "plot.h" // plotting
#include "console.h" // settings

#include <ctype.h> // isdigit
#include <pthread.h> // mutex
#include <math.h> // fmod

static GPU_Target *target;
static SDL_Window *win;

static font_s font;

rectf cam;
pthread_mutex_t renderer_mutex;

static uint8_t *key_state, *key_state_last;
static unsigned num_keys;

#define KEY_HOLD(K) (key_state[K])
#define KEY_PRESS(K) (key_state[K] && !key_state_last[K])

const unsigned font_encode(const char c) {
	if (isdigit(c)) return c-'0'+3;
	else if (c == '.') return 2;
	else if (c == '-') return 1;
	else return 0;
}

int window_init() {

	// Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		EXCEPT("SDL Initialization error\n");
		error_throw("SDL failed to initialize");
		return 0;
	}

	// Create a window supporing OpenGL 
	win = SDL_CreateWindow("JaPlot calculator 0.1", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, settings.WIDTH, settings.HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	// Create and OpenGL context
	GPU_SetInitWindow(SDL_GetWindowID(win));
	target = GPU_Init(settings.WIDTH, settings.HEIGHT, GPU_DEFAULT_INIT_FLAGS);

	// enable vsync
	SDL_GL_SetSwapInterval(1); 

	// Set the icon
    SDL_Surface *icon = SDL_LoadBMP("../res/icon.bmp");
    SDL_SetWindowIcon(win, icon);
    SDL_FreeSurface(icon);

	// Initialize the keyboard interface
    key_state = (uint8_t*)SDL_GetKeyboardState(&num_keys);
    key_state_last = calloc(num_keys, sizeof(uint8_t));

	// Load the font bitmap
	font = font_load("../res/freesans.png", 1, 13, settings.col_text);

	// Init camera
    //cam = {-3.0,-3.0, 6.0, 6.0};
	cam.x = -3.0;
	cam.y = -3.0;
	cam.w =  6.0;
	cam.h =  6.0;

	// Initialize the renderer mutex
	if (pthread_mutex_init(&renderer_mutex, NULL)) {
		EXCEPT("An error occured whilst creating the renderer mutex.\n");
	}

	return 1;
}

int window_destroy() {
	GPU_Quit();
    SDL_DestroyWindow(win);
	SDL_Quit();

	font_destroy(font);

	target = NULL;
	win = NULL;

	return 1;
}

int window_update() {

	// Resize the window if needed
	{
		unsigned prevw = settings.WIDTH, prevh = settings.HEIGHT;
		SDL_GetWindowSize(win, &settings.WIDTH, &settings.HEIGHT);
		
		if (prevw != settings.WIDTH || prevh != settings.HEIGHT) {
			settings.WIDTH = settings.WIDTH < 250 ? 250 : settings.WIDTH;
			settings.HEIGHT = settings.HEIGHT < 250 ? 250 : settings.HEIGHT;

			GPU_SetWindowResolution(settings.WIDTH, settings.HEIGHT);
		}
	}

	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
			case SDL_QUIT :
				return 0;
			break;
		}
	}

	pthread_mutex_lock(&renderer_mutex);

	if (!KEY_HOLD(SDL_SCANCODE_LCTRL)) {
		if (KEY_HOLD(SDL_SCANCODE_LEFT))  cam.x -= settings.cam_movespeed*cam.w/5.0;
		if (KEY_HOLD(SDL_SCANCODE_RIGHT)) cam.x += settings.cam_movespeed*cam.w/5.0;
		if (KEY_HOLD(SDL_SCANCODE_UP))    cam.y -= settings.cam_movespeed*cam.h/5.0;
		if (KEY_HOLD(SDL_SCANCODE_DOWN))  cam.y += settings.cam_movespeed*cam.h/5.0;
	} else {
		rectf prevcam = cam;

		if (KEY_HOLD(SDL_SCANCODE_LEFT) /*&& !(cam.w > 100)*/)
			cam.w*=settings.cam_scalespeed;
		
		if (KEY_HOLD(SDL_SCANCODE_RIGHT) && 1/*!(cam.w < 2)*/)
			cam.w/=settings.cam_scalespeed;
		
		if (KEY_HOLD(SDL_SCANCODE_UP) && 1/*!(cam.w < 2 || cam.h < 2)*/)  {
			cam.w/=settings.cam_scalespeed;
			cam.h/=settings.cam_scalespeed;
		}

		if (KEY_HOLD(SDL_SCANCODE_DOWN) && 1/*!(cam.w > 100 || cam.h > 100)*/)  {
			cam.w*=settings.cam_scalespeed;
			cam.h*=settings.cam_scalespeed;
		}

		cam.x+=(prevcam.w-cam.w)/2;
		cam.y+=(prevcam.h-cam.h)/2;
	}

	pthread_mutex_unlock(&renderer_mutex);

	// LOGIC STUFF
	memcpy(key_state_last, key_state, num_keys*sizeof(key_state[0]));

	return 1;

}

int window_draw() {
	// RENDERING STUFF 
	GPU_ClearColor(target, settings.col_background);

	// Will be useful
	pointi zero = WORLD2CAM(((pointf){0,0}));

	// GRIDLINES
	{
		GPU_SetLineThickness(1.0f);

		double wlog = log10(cam.w/4);
		double hlog = log10(cam.h/4);

		pointf step = {pow(10.0, floor(wlog)), pow(10.0, floor(hlog))};
		pointf p = (pointf){cam.x-fmod(cam.x, step.x), 
							cam.y-fmod(cam.y, step.y)};
		pointi pcam = {0};
		while (p.x <= cam.x+cam.w || p.y <= cam.y+cam.h) {
			pcam = WORLD2CAM(p);

			SDL_Color coldarker = COLDARKER1(settings.col_grid);

			//GPU_SetLineThickness(fmod(p.x, step.x*5) ? 1.0f : 2.0f);
			GPU_Line(target, pcam.x, 0, pcam.x, settings.HEIGHT, ((long)abs(round(p.x/step.x)) % 5 == 0) ? coldarker : settings.col_grid);

			//GPU_SetLineThickness(fmod(p.y, step.y*5) ? 1.0f : 2.0f);
			GPU_Line(target, 0, pcam.y, settings.WIDTH, pcam.y, ((long)abs(round(p.y/step.y)) % 5 == 0) ? coldarker : settings.col_grid);

			p.x += step.x;
			p.y += step.y;
		}

		// reset these back
		p = (pointf){cam.x-fmod(cam.x, step.x), 
					 cam.y-fmod(cam.y, step.y)};
		pcam = (pointi){0, 0};

		// number drawing stuff
		char nums[20];

		pointf modlog = {fmod(wlog, 1.0), fmod(hlog, 1.0)};

		pointi numstep = {(wlog < 0.0 ? "521" : "125")[(long)(floor(fabs(modlog.x)*3.0))]-'0',
						  (hlog < 0.0 ? "521" : "125")[(long)(floor(fabs(modlog.y)*3.0))]-'0'};
		while (p.x <= cam.x+cam.w || p.y <= cam.y+cam.h) {
			pcam = WORLD2CAM(p);

			// Draw numbers
			if ((long)abs(round(p.x/step.x)) % numstep.x == 0) {
			if (p.x != 0.0) { // We dont want the 0s to overlap
				sprintf(nums, "%.*f", wlog > 0.0 ? 0 : abs((int)floor(wlog)), p.x);
				font_draw_string(target, pcam.x, zero.y+10, 0.25f, font, nums, font_encode);
			}
			}

			//if (fmod(p.y/step.y, step.y*numstep.y) == 0) {
			if ((long)abs(round(p.y/step.y)) % numstep.y == 0) {
				sprintf(nums, "%.*f", hlog > 0.0 ? 0 : abs((int)floor(hlog)), -p.y);
				font_draw_string(target, zero.x+6, pcam.y+10, 0.25f, font, nums, font_encode);
			}
		
			p.x += step.x;
			p.y += step.y;
		}
	}

	// Draw the 0,0 cross
	GPU_SetLineThickness(3.0f);
	GPU_Line(target, 0, zero.y, settings.WIDTH, zero.y, COLDARKER2(settings.col_grid));
	GPU_Line(target, zero.x, 0, zero.x, settings.HEIGHT, COLDARKER2(settings.col_grid));

	//Graph and plot all sets
	pthread_mutex_lock(&renderer_mutex);
	for (set_s* s = set_first; s != NULL; s = s->next) {

		if (s->plot_type == PT_FUNCTION)
			//graph(cam.x, cam.x+cam.w, settings.WIDTH/4, s);
			graph(cam.x, cam.x+cam.w, SET_MAXLENGTH, s);

		plot(target, s);
	}
	pthread_mutex_unlock(&renderer_mutex);

	GPU_Flip(target);

	return 1;
}

