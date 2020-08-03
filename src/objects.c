#include "objects.h"

#include <string.h> //strlen
#include <stdlib.h> // malloc, free
#include <ctype.h> // isalpha
#include <math.h> // hardcoded math functions etc.

#include "SDL.h" // Color etc.
#include "error.h"
#include "console.h" // console colors

static ds_trie* trie_objects;
set_s* set_first = NULL;
static set_s* set_last = NULL;

static unsigned instances[5], limits[5] = {50, 50, 25, 25, 8};

// ---------INITIALIZATION STUFF ------------

// defined all the way down in this file
static error_t generic_free(object* obj);

static double sgn(double x) { return (x > 0.0 ? 1.0 : x < 0.0 ? -1.0 : 0.0); }

void objects_init() {
	trie_objects = trie_create();	

	object_add("sin", OT_CFUNC, sin);	
	object_add("cos", OT_CFUNC, cos);	
	object_add("sqrt", OT_CFUNC, sqrt);	
	object_add("abs", OT_CFUNC, fabs);
	object_add("sgn", OT_CFUNC, sgn);
	object_add("PI", OT_CONSTANT, &(double){M_PI});
	object_add("e", OT_CONSTANT, &(double){M_E});
}

void objects_destroy() {
	trie_destroy(trie_objects, (void (*)(void*))generic_free);
	trie_objects = NULL;
}

// Variable/Function/Set adding & removing procedures

static const unsigned char trie_encode(const char c) {
	if ((unsigned char)c >= 33)
		return c-33;	
	else
		return 0;
}

static const char trie_decode(const unsigned char c) {
	return c+33;
}

// CHECK THE VALIDITY OF AN OBJECT NAME
static error_t checkname(const char* name) {

	// Check if name isn't empty
	if (name == NULL || *name == '\0') {
		error_throw("name not specified");		
		return ERROR_CODE_FAIL;
	}

	// Check if name isn't too long
	if (strlen(name)+1 > NAME_MAXLEN) {
		error_throw_val("maximum name length is %lu", NAME_MAXLEN-1); 
		return ERROR_CODE_FAIL;
	}

	// Check if object with the same name doesn't already exist
	if (trie_find(trie_objects, name, trie_encode) != NULL) {
		error_throw("an object with the same name already exists");
		return ERROR_CODE_FAIL;
	}

	// Check if name starts with a letter
	if (!isalpha(*name)) {
		error_throw("names must start with letters");
		return ERROR_CODE_FAIL;
	} 

	// check if string is alphabetic only
	for (const char* c = name; *c; c++)
		if (!isalnum(*c) && *c != '_') {
			error_throw("only alphabetic characters are allowed");
			return ERROR_CODE_FAIL;
		}

	return ERROR_CODE_OK;
		
}	

// Add a generic object to the trie structure
error_t object_add(const char* name, int type, void* data) {

	if (instances[type]+1 > limits[type]) {
		error_throw_str("the maximum number of %ss has been reached", obj_type_str(type));
		return 0;
	}

	if (ERROR_FAIL(checkname(name))) return ERROR_CODE_FAIL;

	object* obj = malloc(sizeof(object));
	obj->type = type;
	obj->hidden = 0;

	// This is copying from pointer to stack variable so you dont have to free if checkname fails
	size_t size;
	switch (type) { 
		case OT_VARIABLE :
		case OT_CONSTANT : size = sizeof(double);    break;
		case OT_FUNCTION : size = sizeof(formula_s); break;
		case    OT_CFUNC : size = 0;                 break;
		case      OT_SET : size = sizeof(set_s);       break;

		default : 
			free(obj);
			error_throw("corrupted object type");
			return ERROR_CODE_FAIL;
		break;
	}

	if (type != OT_CFUNC) {
		obj->data = malloc(size);
		memcpy(obj->data, data, size);
	} else obj->cfunc = data;

	trie_add(trie_objects, name, trie_encode, obj);

	// add to the linked list of sets
	if (type == OT_SET) {

		if (set_last != NULL)
			set_last->next = obj->set;

		obj->set->prev = set_last;
		obj->set->next = NULL;

		set_last = obj->set;

		if (set_first == NULL)
			set_first = set_last;
	}

	instances[type]++;

	return ERROR_CODE_OK;
}

// Free (deallocate) the generic object completely
static error_t generic_free(object* obj) {
	
	if (obj == NULL){
		error_throw("invalid object");
		return ERROR_CODE_FAIL;
	}

	switch (obj->type) { 
		case OT_CONSTANT : 
		case OT_VARIABLE : 
			free(obj->val);
	    break;
		case OT_FUNCTION : 
			free(obj->func->toks); 

			free(obj->func);
		break;
		case OT_CFUNC :

		break;
		case OT_SET : 
	
			// Removing node from linked list
			if (obj->set->prev != NULL)
				obj->set->prev->next = obj->set->next;
			if (obj->set->next != NULL)
				obj->set->next->prev = obj->set->prev;

			if (obj->set->prev == NULL)
				set_first = obj->set->next;
			if (obj->set->next == NULL)
				set_last = obj->set->prev;

			free(obj->set->coords);
			free(obj->set->formula.toks);

			free(obj->set);
		break;
		default :
			error_throw("invalid object type");
			return ERROR_CODE_FAIL;
		break;
	}

	instances[obj->type]--;
	free(obj);

	return ERROR_CODE_OK; 
	
}

static bool formula_contains(formula_s formula, const char* name) {
	if (formula.toks == NULL) return 0;

	for (size_t tok = 0; tok < formula.numtoks; tok++)
		if (formula.toks[tok].type == TT_VARIABLE ||
			formula.toks[tok].type == TT_FUNCTION)
			if (strcmp(formula.toks[tok].name, name) == 0) {
				return 1;
			}

	return 0;
}

// generic remove
error_t object_remove(const char* name) {

	// First, check if any set or function uses the variable/constant/function so we don't mess things up
	ds_vector* dumps[5];
	objects_dump(dumps); 

	// Check the graphs first
	for (size_t i = 0; i < vector_length(dumps[OT_SET]); i++) {
		ds_trie_dump* dump = (ds_trie_dump*)vector_get(dumps[OT_SET], i);
		formula_s formula = ((object*)dump->data)->set->formula;

		if (formula_contains(formula, name)) 	{
			error_throw_str("Object is used in the graph "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_RESET, dump->name );
			return ERROR_CODE_FAIL;
		}
	}

	for (size_t i = 0; i < vector_length(dumps[OT_FUNCTION]); i++) {
		ds_trie_dump* dump = (ds_trie_dump*)vector_get(dumps[OT_FUNCTION], i);
		formula_s formula = *((object*)dump->data)->func;

		if (formula_contains(formula, name)) 	{
			error_throw_str("Object is used in the function "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_RESET, dump->name );
			return ERROR_CODE_FAIL;
		}
	}

	return generic_free(trie_remove(trie_objects, name, trie_encode));	
}

error_t object_rename(const char* name, const char* newname) {

	if (ERROR_FAIL(checkname(newname)))
		return ERROR_CODE_FAIL;

	object* obj = trie_remove(trie_objects, name, trie_encode);
	if (obj == NULL){
		error_throw("invalid object");
		return ERROR_CODE_FAIL;
	}

	trie_add(trie_objects, newname, trie_encode, obj);	

	return ERROR_CODE_OK;
}

error_t object_hide(const char* name, _Bool val) {
	object* obj = trie_find(trie_objects, name, trie_encode);
	if (obj == NULL) {
		error_throw_str("object " ANSI_COLOR_YELLOW "'%s'" ANSI_COLOR_RESET " not found", name);
		return ERROR_CODE_FAIL;
	}

	if (obj->hidden == val) {
		error_throw_str("object is already %s", val ? "hidden" : "shown");
		return ERROR_CODE_FAIL;
	}
	
	obj->hidden = val;	

	if (obj->type == OT_SET)
		obj->set->shown = !val;

	return ERROR_CODE_OK;
}

error_t objects_dump(ds_vector** arr) {
	ds_vector* vec = trie_dump(trie_objects, trie_decode);
	if (vec == NULL) {
		error_throw("dump error");
		return ERROR_CODE_FAIL;
	}

	for (size_t i = 0; i < 5; i++) 
		arr[i] = vector_create(vec->destroy_element);

	for (size_t i = 0; i < vector_length(vec); i++) {
		ds_trie_dump* obj_dump = vector_get(vec, i);
		object* obj = (object*)(obj_dump->data);
		
		if (vector_append(arr[obj->type], obj_dump) != DASH_OK) {
			error_throw("vector error");
			vector_destroy(vec);
			return ERROR_CODE_FAIL;
		}
		
	}

	// we dont want to destroy the dumped objects, just the array header
	vec->destroy_element = NULL;
	vector_destroy(vec);
	return ERROR_CODE_OK;
}

error_t object_get(const char* name, object** obj) {
	object* tmpobj = trie_find(trie_objects, name, trie_encode);

	if (tmpobj == NULL) {
		error_throw_str("object " ANSI_COLOR_YELLOW "'%s'" ANSI_COLOR_RESET " not found", name);
		return ERROR_CODE_FAIL;
	}

	if (tmpobj->hidden) {
		error_throw_str("object " ANSI_COLOR_YELLOW "'%s'" ANSI_COLOR_RESET " is hidden", name);
		return ERROR_CODE_FAIL;
	}
	
	if (obj) 
		*obj = tmpobj;

	return ERROR_CODE_OK;
}

// ---------- SET HANDELING -------------------
error_t graph_add(const char* name, formula_s formula, SDL_Color col) {

	set_s s = {
		.plot_type = PT_FUNCTION,
	
		.coords = calloc(sizeof(pointf), SET_MAXLENGTH),
		.length = 0,

		.formula = formula,
		.linewidth = 2,
		.shown = 1,
		.col_line = col
	};

	error_t retval = object_add(name, OT_SET, &s);
	if (ERROR_FAIL(retval)) free(s.coords);

	return retval;
}

error_t plot_add(const char* name, pointf* coords, size_t length, SDL_Color col) {

	set_s s = {
		.plot_type = PT_LINEAR,

		.coords = coords,
		.length = length,

		.linewidth = 2,
		.shown = 1,
		.col_line = col
	};

	error_t retval = object_add(name, OT_SET, &s);
	if (ERROR_FAIL(retval)) free(s.coords);

	return retval;
}

const char* obj_type_str(int type) {
	static const char *names[5] = {"constant", "variable", "function", "plugin function", "set"};
	return names[type];
}
