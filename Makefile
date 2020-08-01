#this was tested on Ubuntu 18.04.4 LTS with SDL 2.0.10

CC=gcc
name=./bin/run_linux
src=src/*.c
SDL_CONFIG ?= /usr/local/bin/sdl2-config

all :
	${CC} ${src} -I./include -I${DASH_PATH}/include -L${DASH_PATH}/lib `${SDL_CONFIG} --cflags --libs` -lSDL2_gpu -lSDL2 -lm -ldl -pthread -ldash -o ${name}

Debug : all

Release : all
