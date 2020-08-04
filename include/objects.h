#pragma once

#include "dash/trie.h"
#include "dash/vector.h"
#include "error.h"

typedef struct formula_s formula_s;
#include "parser.h" // token

typedef struct set_s set_s;
#include "plot.h" // pointf

#include "SDL.h" // color

#define SET_MAXLENGTH 2048LU
#define SETS_MAXNUM 2LU

typedef struct formula_s {
    token* toks;
    size_t numtoks;
} formula_s;

typedef struct set_s {
    struct set_s *next, *prev; // this is actually a linked list node

    pointf *coords;
    size_t length;

    formula_s formula;

    _Bool shown;
    SDL_Color col_point, col_line;
    unsigned pointrad;
    unsigned linewidth;
    enum {
        PT_FUNCTION, PT_POINTS, PT_LINEAR, PT_CUBIC, PT_SHARP_IN, PT_SHARP_OUT
    } plot_type;
} set_s;

// A generic object
typedef struct object {

    enum {
        OT_CONSTANT = 0, OT_VARIABLE, OT_FUNCTION, OT_CFUNC, OT_SET
    } type;

    _Bool hidden;

    union {
        void* data; // GENERIC ACCESS

        double (*cfunc)(double); // OT_CFUNC
        double* val; //OT_CONSTANT, OT_VARIABLE
        formula_s* func; // OT_FUNCTION
        set_s* set; //OT_SET
    };

} object;

extern set_s* set_first; // linked list for easy iterations

void objects_init();
void objects_destroy();

// OBJECT STUFF

error_t object_remove(const char* name);
error_t object_rename(const char* name, const char* newname);
error_t object_hide(const char* name, _Bool val);
error_t objects_dump(ds_vector** arr); 

error_t graph_add(const char* name, formula_s formula, SDL_Color col);
error_t plot_add(const char* name, pointf* coords, size_t length, SDL_Color col);

error_t object_add(const char* name, int type, void* copy);
error_t object_get(const char* name, object** obj);

const char* obj_type_str(int type);
