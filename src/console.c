#include "dash/trie.h"
#include "dash/vector.h"

#include "parser.h" // lex
#include "error.h" // error_catch
#include "console.h" // settings
#include "objects.h" // var_add etc.

#include "renderer.h" // accessing the camera

#include <string.h> // nice string functions
#include <stdio.h> // printf

#include <stdlib.h> //random (random colors)
#include <time.h>

#include <ctype.h> // isspace

#include <dlfcn.h> // dynamic loading (plugins)

//static const char* whitespace = " \t\n\v\f\r";

settings_s settings = {
    .grid_size = 1.0,
    .cam_movespeed = 0.10,
    .cam_scalespeed = 1.05,
    .col_grid = (SDL_Color){200,200,200,200},
    .col_background = (SDL_Color){240,240,240,255},
    .col_text = (SDL_Color){120,120,120,255},

	.WIDTH = 500,
	.HEIGHT = 500
};


static const struct {SDL_Color color; const char name[8]; } pallete[] = {
	{{255,   0,   0, 255}, "RED"},
	{{  0,   0, 255, 255}, "BLUE"},
	{{ 40, 255,  40, 255}, "GREEN"},
	{{255, 255,   0, 255}, "YELLOW"},
	{{255,   0, 255, 255}, "MAGENTA"},
	{{255, 150,   0, 255}, "ORANGE"},
	{{  0,   0,   0, 255}, "BLACK"},
	{{255, 255, 255, 255}, "WHITE"},
	{{127, 127, 127, 255}, "GREY"},
	{{  0, 100, 255, 255}, "CYAN"},
	{{  0, 255,   0, 255}, "LIME"},
	{{127,  50,   0, 255}, "BROWN"}
};

static ds_trie* trie_colors; 
static ds_trie* trie_commands;
static ds_trie* trie_plugins;

typedef int (*console_function)();

// = = = = TRIE STUFF = = = =
// The trie node has only 100 branches maximum
static const unsigned char trie_encode(const char c) {
	if ((unsigned char)c >= 33)
		return c-33;	
	else
		return 0;
}

// = = = = HELPER FUNCTIONS = = = =

static char* nextarg(char* start) {
	static char* tok = NULL;
	if (start != NULL) tok = start;
	if (tok == NULL) return NULL;

    while (isspace(*tok)) tok++;

	start = tok;
    if (tok[0] == '"') {
		start++; // move past the '<'
    	if ((tok = strchr(start, '"')) == NULL) return NULL;
	} else {
    	while (*tok != '\0' && !isspace(*tok)) tok++;
	}

	if (*tok == '\0') tok = start = NULL;
	else {
		*tok = '\0';
		tok++; // advance to next
	}

	return start;
}

static const SDL_Color* nextcolor() {
	static size_t currcol = 0;
	
	return &pallete[(currcol++) % (sizeof(pallete)/sizeof(*pallete))].color;
}

static const char* path_to_name(const char* path) {
	// this code does "~/jpplugins/plugin.so" -> "plugin"
	for (const char* c = path; *c; c++)
		if (*c == '\\' || *c == '/') 
			path = c+1;	

	// Ignore the extension
	char* dot = strchr(path, '.');
	if (dot) *dot = '\0';

	return path;
}

// = = = = ACTUAL CONSOLE FUNCTIONS = = = = 
// csfn - ConSole FuNction
// I use nextarg(NULL) (similar to strtok) to get additional arguments for individual command/res
// the trie only cares about the actual commands (graph, plot ... )
#define ASSERT(val, msg) if (!(val)) { printf(ANSI_COLOR_RED "Assertion failed : " ANSI_COLOR_RESET "%s\n", msg); return ERROR_CODE_FAIL; }
#define ASSERT_EX(val, msg) if (!(val)) { printf(ANSI_COLOR_RED "Assertion failed : " ANSI_COLOR_RESET "%s\n", msg); goto exit; }
#define REQUIRE_ARG(str) {const char* _arg = nextarg(NULL); ASSERT(_arg && strcmp(_arg, str) == 0, ANSI_COLOR_YELLOW"'"str"'"ANSI_COLOR_RESET" token expected")}
#define REQUIRE_ARG_EX(str) {const char* _arg = nextarg(NULL); ASSERT_EX(_arg && strcmp(_arg, str) == 0, ANSI_COLOR_YELLOW"'"str"'"ANSI_COLOR_RESET" token expected")}
 

// When a function is labeled as "safe", it means that it catches all errors
// thus no need to ERROR_MSG when calling it

static error_t safe_lex(const char* func, formula_s *formula, _Bool should_validate) {

	ASSERT(func, "missing function definition");

	*formula = lex(func);

	if (formula->toks == NULL) { 
		ERROR_MSG("lexing");
		return ERROR_CODE_FAIL;
	}

	if (should_validate && ERROR_FAIL(validate(*formula))) {
		ERROR_MSG("verifying");
		return ERROR_CODE_FAIL;
	}

	return ERROR_CODE_OK;
}

static error_t safe_compute(const char* func, double* result) {
	ASSERT(func, "missing function definition");

	formula_s formula = lex(func);
	if (formula.toks == NULL) { 
		ERROR_MSG("lexing");
		return ERROR_CODE_FAIL;
	}

	double val;
	if (ERROR_FAIL(compute(&val, formula, NULL))) {
		ERROR_MSG("computing");

		free(formula.toks);
		return ERROR_CODE_FAIL;
	}

	free(formula.toks);
	*result = val;
	return ERROR_CODE_OK;
}

static _Bool isnumber(const char* str, _Bool point) {
	for (const char* c = str; *c; c++) {
		if (!isspace(*c) && !isdigit(*c) && *c != '-' && !(point && *c == '.')) {
			return 0;
		}

		if (*c == '.') point = 0;
	}

	return 1;
}

static error_t safe_atof(double* dst, const char* str) {
	if (!isnumber(str, 1)) {
		error_throw("input value is not a number");	
		ERROR_MSG("parsing");
		return ERROR_CODE_FAIL;
	}

	*dst = atof(str);	
	return ERROR_CODE_OK;
}

static error_t safe_getobj(object** dst, const char* name) {
	if (ERROR_FAIL(object_get(name, dst))) {
		ERROR_MSG("searching for an object");
		return ERROR_CODE_FAIL;
	} 

	return ERROR_CODE_OK;
}

static error_t add_var(_Bool isconst) {
	const char* var_name = nextarg(NULL);

	ASSERT(var_name, "missing variable name");

	ASSERT(strcmp(var_name, "x") != 0, "'x' is a reserved keyword and cannot be used");	

	REQUIRE_ARG("=");

	double result;
	if(ERROR_FAIL(safe_compute(nextarg(NULL), &result)))
		return ERROR_CODE_FAIL;

	if (ERROR_FAIL(object_add(var_name, isconst ? OT_CONSTANT : OT_VARIABLE, &result))) {
		ERROR_MSG("adding a variable");
		return ERROR_CODE_FAIL;	
	}

	printf(ANSI_COLOR_GREEN "Variable "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_GREEN" (%.2lf) added\n" ANSI_COLOR_RESET, var_name, result);

	return ERROR_CODE_OK;
}

static error_t csfn_addconst() {
	return add_var(1);
}

static error_t csfn_addvar() {
	return add_var(0);
}

static error_t getset(object** dst, const char** name) {
	const char* set_name = nextarg(NULL);
	ASSERT(set_name, "no set specified");

	object* obj; 
	if (ERROR_FAIL(safe_getobj(&obj, set_name)))
		return ERROR_CODE_FAIL;

	if (obj->type != OT_SET) {
		error_throw_str("object '%s' is not a set", set_name);
		ERROR_MSG("searching for a set");
		return ERROR_CODE_FAIL;
	}

	*dst = obj;
	*name = set_name;

	return ERROR_CODE_OK;
}

static error_t csfn_color() {
	object* obj;
	const char* name;
	if (ERROR_FAIL(getset(&obj, &name)))
		return ERROR_CODE_FAIL;

	const char* arg = nextarg(NULL);
	SDL_Color* found = trie_find(trie_colors, arg, trie_encode);
	if (found == NULL) {
		printf(ANSI_COLOR_RED "The specified color ("ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_RED") was not found.\n" ANSI_COLOR_RESET, arg);
		return ERROR_CODE_FAIL;
	}

	obj->set->col_line = *found;

	printf(ANSI_COLOR_GREEN "The color of "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_GREEN" was successfully changed\n", name);
	return ERROR_CODE_OK;
}

static error_t csfn_line() {
	object* obj;
	const char* name;
	if (ERROR_FAIL(getset(&obj, &name)))
		return ERROR_CODE_FAIL;

	const char* arg = nextarg(NULL);
	if (!isnumber(arg, 0)) {
		error_throw("input value is not a number");
		ERROR_MSG("parsing");
		return ERROR_CODE_FAIL;
	}

	obj->set->linewidth = atoi(arg);
	if (obj->set->linewidth > 8) obj->set->linewidth = 8;

	printf(ANSI_COLOR_GREEN "The line thickness of "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_GREEN" was successfully changed\n", name);
	return ERROR_CODE_OK;
}

static error_t csfn_mod() {
	const char* obj_name = nextarg(NULL);
	ASSERT(obj_name, "no object specified");

	object* obj; 
	if (ERROR_FAIL(safe_getobj(&obj, obj_name)))
		return ERROR_CODE_FAIL;

	//REQUIRE_ARG("to");

	const char* arg = nextarg(NULL);;
	ASSERT(arg, "Missing expression");
	
	switch (obj->type) {
		case OT_VARIABLE :
			if (ERROR_FAIL(safe_compute(arg, obj->val)))
				return ERROR_CODE_FAIL;

			printf(ANSI_COLOR_GREEN "Variable " ANSI_COLOR_YELLOW "'%s'" ANSI_COLOR_GREEN " (%.2lf) modified\n" ANSI_COLOR_RESET, obj_name, *obj->val);
		break;
		case OT_CONSTANT :
			error_throw("cannot modify constants");
			ERROR_MSG("modifying");
			return ERROR_CODE_FAIL;
		break;
		case OT_FUNCTION :
			if (ERROR_FAIL(safe_lex(arg, obj->func, 0)))
				return ERROR_CODE_FAIL;

			printf(ANSI_COLOR_GREEN "Function " ANSI_COLOR_YELLOW "'%s'" ANSI_COLOR_GREEN " modified\n" ANSI_COLOR_RESET, obj_name);
		break;
		default :
			error_throw("invalid object type");
			ERROR_MSG("modifying");
			return ERROR_CODE_FAIL;
		break;
	}

	return ERROR_CODE_OK;
}

static error_t csfn_graph() {
	const char *args[2] = {nextarg(NULL), nextarg(NULL)};
	const char *form;
	char namebuf[NAME_MAXLEN];

	if (args[1] != NULL && strcmp(args[1], "=") == 0) {
		form = nextarg(NULL);	
		strcpy(namebuf, args[0]);
	} else {
		form = args[0];

		static unsigned gnum = 0;

		// Generate a new name until it is not already taken
		do
			sprintf(namebuf, "g%u", gnum++);
		while (!ERROR_FAIL(object_get(namebuf, NULL)));
	}

	ASSERT(namebuf[0], "Missing set name");
	ASSERT(form, "Missing function definition");

	formula_s formula;
	if (ERROR_FAIL(safe_lex(form, &formula, 1)))
		return ERROR_CODE_FAIL;

	SDL_Color color = *nextcolor();

	if (ERROR_FAIL(graph_add(namebuf, formula, color))) {
		ERROR_MSG("adding a set");	
	
		free(formula.toks);
		return ERROR_CODE_FAIL;
	}

	printf(ANSI_COLOR_GREEN "Set "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_GREEN" added\n" ANSI_COLOR_RESET, namebuf);
	return ERROR_CODE_OK;	
}	

static error_t csfn_plot() {
	const char *args[2] = {nextarg(NULL), nextarg(NULL)};
	const char *filename;
	char namebuf[NAME_MAXLEN];

	if (args[1] != NULL && strcmp(args[1], "<") == 0) {
		filename = nextarg(NULL);	
		strcpy(namebuf, args[0]);
	} else {
		filename = args[0];

		static unsigned pnum = 0;

		// Generate a new name until it is not already taken
		do
			sprintf(namebuf, "p%u", pnum++);
		while (!ERROR_FAIL(object_get(namebuf, NULL)));
	}

	ASSERT(namebuf[0], "Missing plot name");
	ASSERT(filename, "Missing file name");

	FILE* file = fopen(filename, "r");
	ASSERT(file, "cannot open file");

	pointf* coords = calloc(sizeof(pointf), SET_MAXLENGTH);
	pointf* lastp = coords;
	
	while (lastp-coords < SET_MAXLENGTH) {
		if (fscanf(file, "%lf %lf", &lastp->x, &lastp->y) != 2) break;
		lastp++;
	}

	if (!feof(file))
		printf(ANSI_COLOR_RED"Only a portion of the points has been plotted (the limit is %lu points)\n", SET_MAXLENGTH);

	// sort the points based on their x value
	qsort(coords, lastp-coords, sizeof(pointf), (int (*)(const void*, const void*))pointf_compare);

	SDL_Color color = *nextcolor();

	if (ERROR_FAIL(plot_add(namebuf, coords, lastp-coords, color))) {
		ERROR_MSG("adding a set");	
		fclose(file);
		return ERROR_CODE_FAIL;
	}

	fclose(file);

	printf(ANSI_COLOR_GREEN "Set "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_GREEN" added\n" ANSI_COLOR_RESET, namebuf);

	return ERROR_CODE_OK;
}

static error_t csfn_compute() {
	const char* arg = nextarg(NULL);
	const char* func;
	FILE* out = stdout;

	if (strcmp(arg, ">") == 0) {
		const char* filename = nextarg(NULL);
		ASSERT(filename, "File name not specified");
		
		out = fopen(filename, "w");
		ASSERT(out, "cannot open file");

		func = nextarg(NULL);
	} else func = arg;	

	_Bool is_ranged = 0;
	double range_start = 0;
	double range_end = 0;
	double range_step = 0;

	// saving the variables so we know which ones to delete
	ds_vector* var_names = NULL;
	const char* for_kw = nextarg(NULL);
	if (for_kw && strcmp(for_kw, "for") == 0) {

		var_names = vector_create((int (*)(void*))object_remove); 

		while ((arg = nextarg(NULL)) != NULL) {
			const char* name = arg;

			REQUIRE_ARG_EX("=");

			double val;
			if (ERROR_FAIL(safe_compute(nextarg(NULL), &val)))
				goto exit;

			// with x you can do range stuff (x = 0 .. 1 + 0.1)
			if (strcmp(name, "x") == 0 && (arg = nextarg(NULL)) != NULL && strcmp(arg, "..") == 0) {
				is_ranged = 1;

				range_start = val;

				const char* args[2];
				args[0] = nextarg(NULL);

				ASSERT_EX(args[0], "Missing function definition");

				REQUIRE_ARG_EX("+");

				args[1] = nextarg(NULL);

				if (ERROR_FAIL(safe_compute(args[0], &range_end)) |
					ERROR_FAIL(safe_compute(args[1], &range_step)))
					goto exit;
			} else {
				if (ERROR_FAIL(object_add(name, OT_VARIABLE, &val))) {
					ERROR_MSG("adding a variable");
					goto exit;
				}
				vector_append(var_names, (void*)name);
			}

			if ((arg = nextarg(NULL)) == NULL) break;
			ASSERT_EX(strcmp(arg, ",") == 0, ANSI_COLOR_YELLOW"','"ANSI_COLOR_RESET" token expected");
		}	
	}

	if (!is_ranged) {
		double result;
		if (ERROR_FAIL(safe_compute(func, &result)))
			goto exit;

		if (out == stdout)
			fprintf(out, ANSI_COLOR_GREEN "%.2lf\n" ANSI_COLOR_RESET, result);
		else
			fprintf(out, "%lf\n", result);

	} else {
		formula_s formula;
		if (ERROR_FAIL(safe_lex(func, &formula, 1)))
			goto exit;
		
		size_t i = 0;
		for (double x = range_start; x <= range_end; x += range_step, i++) {
			if (i == SET_MAXLENGTH) {
				error_throw_val("range is too big, max size is %ld", SET_MAXLENGTH);	
				ERROR_MSG("computing");
				goto exit;
			}

			double val;
			compute(&val, formula, &x);

			if (out == stdout)
				fprintf(out, ANSI_COLOR_YELLOW"["ANSI_COLOR_GREEN"%.2lf, %.2lf"ANSI_COLOR_YELLOW"]\n"ANSI_COLOR_RESET, x, val);
			else
				fprintf(out, "%lf %lf\n", x, val);
				
		}

		printf(ANSI_COLOR_GREEN "%lu total values calculated\n"ANSI_COLOR_RESET, i);
		free(formula.toks);
	}

	if (out != stdout) fclose(out);
	vector_destroy(var_names);

	return ERROR_CODE_OK;

	// Having all the cleanup code here so it doesn't have to be repeated
	exit :

	if (out != stdout) fclose(out);
	vector_destroy(var_names);

	return ERROR_CODE_FAIL;
}

static int csfn_remove() {
	const char* name = nextarg(NULL);

	// Properly unplug if the object happens to be a plugin function
	void* plugin;
	if ((plugin = trie_remove(trie_plugins, name, trie_encode)) != NULL)
		dlclose(plugin);

	if (ERROR_FAIL(object_remove(name))) {
		ERROR_MSG("removing an object");
		return ERROR_CODE_FAIL;
	}

	printf(ANSI_COLOR_GREEN "Object "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_GREEN" removed\n" ANSI_COLOR_RESET, name);
	return ERROR_CODE_OK;
}

static int csfn_addfunc() {
	const char* func_name = nextarg(NULL);
	ASSERT(func_name, "function name not specified");

	ASSERT(strcmp(func_name, "x") != 0, "'x' is a reserved keyword and could interfere with the grapher");	

	REQUIRE_ARG("=");

	formula_s formula;
	if (ERROR_FAIL(safe_lex(nextarg(NULL), &formula, 0)))
		return ERROR_CODE_FAIL;

	if (ERROR_FAIL(object_add(func_name, OT_FUNCTION, &formula))) {
		ERROR_MSG("adding a function");
		return ERROR_CODE_FAIL;
	} 

	printf(ANSI_COLOR_GREEN "Function "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_GREEN" added\n" ANSI_COLOR_RESET, func_name);
	return ERROR_CODE_OK;
}

static error_t csfn_rename() {
	const char* name = nextarg(NULL);
	ASSERT(name, "Object name not specified");

	REQUIRE_ARG("to");

	const char* newname = nextarg(NULL);
	ASSERT(newname, "New object name not specified");

	if (ERROR_FAIL(object_rename(name, newname))) {
		ERROR_MSG("renaming an object");
		return ERROR_CODE_FAIL;
	} 	

	printf(ANSI_COLOR_GREEN "Object "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_GREEN" %s\n" ANSI_COLOR_RESET, name, "renamed");
	return ERROR_CODE_OK;
}

static error_t obj_sethide(_Bool val) {
	const char* name = nextarg(NULL);
	ASSERT(name, "object name not specified");

	if (ERROR_FAIL(object_hide(name, val))) {
		ERROR_MSG("hiding an object");
		return ERROR_CODE_FAIL;
	}

	printf(ANSI_COLOR_GREEN "Object "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_GREEN" %s\n" ANSI_COLOR_RESET, name, val ? "hidden" : "shown");
	return ERROR_CODE_OK;
}

static error_t csfn_hide() {
	return obj_sethide(1);
}

static error_t csfn_show() {
	return obj_sethide(0);
}

static error_t csfn_center() {
	cam.x = -cam.w/2;
	cam.y = -cam.h/2;

	return ERROR_CODE_OK;
}

static error_t csfn_square() {
	
	double ratio = settings.WIDTH/cam.h;
	cam.w *= ratio;

	return ERROR_CODE_OK;
}

static error_t csfn_cam() {

	const char* action = nextarg(NULL);
	ASSERT(action, "Missing camera action name");	

	if (strcmp(action, "move") == 0) {
		const char *x, *y;
		x = nextarg(NULL);
		y = nextarg(NULL);
		ASSERT(x && y, "move coordinates expected");

		pointf newpos;
		if (ERROR_FAIL(safe_atof(&newpos.x, x))) return ERROR_CODE_FAIL;
		if (ERROR_FAIL(safe_atof(&newpos.y, y))) return ERROR_CODE_FAIL;

		cam.x = newpos.x-cam.w/2;
		cam.y = (-newpos.y)-cam.h/2;

		printf(ANSI_COLOR_GREEN "Camera anchored to "ANSI_COLOR_YELLOW"[%.2lf, %.2lf]\n" ANSI_COLOR_RESET, newpos.x, newpos.y);
	} else if (strcmp(action, "scale") == 0) {
		const char *w, *h;
		w = nextarg(NULL);
		h = nextarg(NULL);
		ASSERT(w && h, "scale size expected");

		pointf newsize;
		if (ERROR_FAIL(safe_atof(&newsize.x, w))) return ERROR_CODE_FAIL;
		if (ERROR_FAIL(safe_atof(&newsize.y, h))) return ERROR_CODE_FAIL;

		ASSERT(newsize.x > 0 && newsize.y > 0, "positive scale size expected");

		cam.x -= (newsize.x-cam.w)/2;
		cam.y -= (newsize.y-cam.h)/2;
	
		cam.w = newsize.x;
		cam.h = newsize.y;

		printf(ANSI_COLOR_GREEN "Camera scaled to "ANSI_COLOR_YELLOW"[%.2lf, %.2lf]\n" ANSI_COLOR_RESET, cam.w, cam.h);
	} else {
		printf(ANSI_COLOR_RED "Invalid camera action name : "ANSI_COLOR_YELLOW"'%s'\n"ANSI_COLOR_RESET, action);
		return ERROR_CODE_FAIL;
	}
	
	return ERROR_CODE_OK;
}

static error_t csfn_echo() {
	const char* msg = nextarg(NULL);	
	ASSERT(msg, "Missing echo message");

	printf(ANSI_COLOR_CYAN"%s\n"ANSI_COLOR_RESET, msg);
	return ERROR_CODE_OK;
}

static error_t csfn_set() {
	const char* option = nextarg(NULL);
	ASSERT(option, "No option specified");

	if (strcmp(option, "gridsize") == 0) {
		const char* arg = nextarg(NULL);
		ASSERT(arg, "Value not specified");
		
		settings.grid_size = atof(arg);
	}	

	return ERROR_CODE_OK;	
}

#ifdef _WIN32
	#define PLUGIN_EXTENSION ".dll"
#else
	#define PLUGIN_EXTENSION ".so"
#endif
// format : plug from ~/jpplugins/plugin.so myFunc
static error_t csfn_plug() {

	{const char* from = nextarg(NULL);
	 ASSERT(from && strcmp(from, "from") == 0, "missing 'from' specifier");}

	char plugin_path_buf[COMMAND_MAXLEN] = {0};
	char* plugin_path = nextarg(NULL);
	ASSERT(plugin_path, "plugin path not specified");
	strcpy(plugin_path_buf, plugin_path);
	plugin_path = plugin_path_buf;

	char* dot = strrchr(plugin_path, '.');
	if (!dot || dot[1] == '\\' || dot[1] == '/')
		strcat(plugin_path, PLUGIN_EXTENSION);

	const char* cfunc_name = nextarg(NULL);
	ASSERT(cfunc_name, "missing function name");

	// Load the function from the plugin
	void* dlhandle = dlopen(plugin_path, RTLD_LAZY);
	ASSERT(dlhandle, "plugin not found");

	void* cfunc = dlsym(dlhandle, cfunc_name);
	if (cfunc == NULL) {
		printf(ANSI_COLOR_RED "plugin function not found" ANSI_COLOR_RESET);
		dlclose(dlhandle);
		return ERROR_CODE_FAIL;
	}

	if (ERROR_FAIL(object_add(cfunc_name, OT_CFUNC, cfunc))) {
		ERROR_MSG("plugging");
		dlclose(dlhandle);
		return ERROR_CODE_FAIL;
	}

	trie_add(trie_plugins, cfunc_name, trie_encode, dlhandle);

	printf(ANSI_COLOR_GREEN "Function "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_GREEN" from the plugin "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_GREEN" added\n" ANSI_COLOR_RESET, cfunc_name, plugin_path);

	return ERROR_CODE_OK;
}

static error_t csfn_gccmakeplug() {
	const char* file_name = nextarg(NULL);
	ASSERT(file_name, "file path not specified");
	
	const char* plugin_name = path_to_name(file_name);
	ASSERT(*plugin_name, "file not specified");

	char shell_command[COMMAND_MAXLEN*2+74+1];
	//gcc [file_name] -c -fPIC -o tmp.o && gcc tmp.o -shared -o [plugin_name].dll/so

	sprintf(shell_command, "gcc %s.c -c -fPIC -o ./tmp.o 2> ./tmp && gcc tmp.o -shared -o ./%s%s 2> ./tmp", file_name, plugin_name, PLUGIN_EXTENSION);

	system(shell_command);
	remove("./tmp.o");

	FILE* log = fopen("./tmp", "r");
	if (fseek(log, 0, SEEK_END), ftell(log) != 0) {
		fseek(log, 0, SEEK_SET);

		printf(ANSI_COLOR_RED "Plugin compilation failed" ANSI_COLOR_RESET " : \n");
		while (!feof(log)) putchar(fgetc(log));
		return ERROR_CODE_FAIL;
	}
	fclose(log);
	remove("./tmp");

	printf(ANSI_COLOR_GREEN "Plugin "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_GREEN" compiled\n" ANSI_COLOR_RESET, plugin_name);
	return ERROR_CODE_OK;
}

static void print_name(ds_trie_dump* dump) {
	if (dump == NULL) {
		printf("%-*s", NAME_MAXLEN, "");
		return;
	}

	const object* obj = dump->data;
	const char* color = obj->hidden ? ANSI_COLOR_DYELLOW : ANSI_COLOR_YELLOW;

	printf("%s", color);
	printf("%-*s"ANSI_COLOR_RESET, NAME_MAXLEN, dump->name);
}

static void print_formula(formula_s formula) {
	static const char* opers = "+-*/%^()-";

	printf(ANSI_COLOR_BLUE);
	for (size_t tok = 0; tok < formula.numtoks; tok++) {
		switch (formula.toks[tok].type) {
			case TT_NUMBER :
				printf("%.2lf ", formula.toks[tok].num);	
			break;
			case TT_OPERATOR :
				printf("%c ", opers[formula.toks[tok].oper]);	
			break;
			case TT_VARIABLE :
			case TT_FUNCTION :
				printf("%s ", formula.toks[tok].name);	
			break;
		}
	}
	printf(ANSI_COLOR_RESET);
}

static error_t csfn_list() {
	const char* arg = nextarg(NULL);

	// Dump objects from the trie by type
	ds_vector *objs[5];
	if (ERROR_FAIL(objects_dump(objs))) {
		ERROR_MSG("dumping");
		return ERROR_CODE_FAIL;
	}

	// If not arguments have been passed
	if (arg == NULL) {
		printf(ANSI_COLOR_GREEN);
		printf("%-*s", NAME_MAXLEN, "Constants");
		printf("%-*s", NAME_MAXLEN, "Variables");
		printf("%-*s", NAME_MAXLEN, "Functions");
		printf("%-*s", NAME_MAXLEN, "CFuncs");
		printf("%-*s", NAME_MAXLEN, "Sets");

		// and figure out the longest vector of them
		size_t longest = 0;
		for (size_t i = 0; i < 5; i++) {
			if (vector_length(objs[i]) > longest)
				longest = vector_length(objs[i]);
		}

		printf("%s", ANSI_COLOR_YELLOW);
		for (size_t col = 0; col < longest*5; col++) {
			if (col%5 == 0) putchar('\n');

			ds_trie_dump* dump_obj = vector_get(objs[col%5], col/5);
			print_name(dump_obj);
		}
		printf("%s", ANSI_COLOR_RESET);

	} else if (strcmp(arg, "funcs") == 0) {
		printf(ANSI_COLOR_GREEN"%-*s\n", NAME_MAXLEN, "Functions");

		for (size_t i = 0; i < vector_length(objs[OT_FUNCTION]); i++) {
			ds_trie_dump* dump_obj = vector_get(objs[OT_FUNCTION], i);
			print_name(dump_obj);
			
			print_formula(*((object*)dump_obj->data)->func);
			putchar('\n');
		}
	} else if (strcmp(arg, "cfuncs") == 0) {
		printf(ANSI_COLOR_GREEN"%-*s\n", NAME_MAXLEN, "CFuncs");

		for (size_t i = 0; i < vector_length(objs[OT_CFUNC]); i++) {
			ds_trie_dump* dump_obj = vector_get(objs[OT_CFUNC], i);
			print_name(dump_obj);
			putchar('\n');
		}
	} else if (strcmp(arg, "sets") == 0) {
		printf(ANSI_COLOR_GREEN);
		printf("%-*s", NAME_MAXLEN, "Set");
		printf("%-*s", 14, "Color");
		printf("%-*s", NAME_MAXLEN, "Formula");

		putchar('\n');
		for (size_t i = 0; i < vector_length(objs[OT_SET]); i++) {
			ds_trie_dump* dump_obj = vector_get(objs[OT_SET], i);
			print_name(dump_obj);

			set_s* set = ((object*)dump_obj->data)->set;
			
			printf(ANSI_COLOR_BLUE"[%3u %3u %3u] ", set->col_line.r, set->col_line.g, set->col_line.b);
			print_formula(set->formula);
			
			putchar('\n');
		}

	} else if (strcmp(arg, "vars") == 0) {
		printf(ANSI_COLOR_GREEN"%-*s\n", NAME_MAXLEN, "Variables");

		for (size_t i = 0; i < vector_length(objs[OT_VARIABLE]); i++) {
			ds_trie_dump* dump_obj = vector_get(objs[OT_VARIABLE], i);
			print_name(dump_obj);
			printf(ANSI_COLOR_BLUE"%.2lf", *((object*)dump_obj->data)->val);

			putchar('\n');
		}
	} else if (strcmp(arg, "consts") == 0) {
		printf(ANSI_COLOR_GREEN"%-*s\n", NAME_MAXLEN, "Constants");

		for (size_t i = 0; i < vector_length(objs[OT_CONSTANT]); i++) {
			ds_trie_dump* dump_obj = vector_get(objs[OT_CONSTANT], i);
			print_name(dump_obj);
			printf(ANSI_COLOR_BLUE"%.2lf", *((object*)dump_obj->data)->val);

			putchar('\n');
		}
	} else {
		printf(ANSI_COLOR_RED"Invalid argument"ANSI_COLOR_RESET);
	}

	for (size_t i = 0; i < 5; i++)
		vector_destroy(objs[i]);

	printf(ANSI_COLOR_RESET);
	putchar('\n');
	return ERROR_CODE_OK;
}

// ---- Console initialization ----
static void console(volatile _Atomic _Bool*);

void* console_start(void* arg) {
	srand(time(NULL));

	trie_commands = trie_create();

	trie_add(trie_commands, "cam", trie_encode, csfn_cam);

	trie_add(trie_commands, "echo", trie_encode, csfn_echo);
	trie_add(trie_commands, "set", trie_encode, csfn_set);

	trie_add(trie_commands, "calc", trie_encode, csfn_compute);
	trie_add(trie_commands, "graph", trie_encode, csfn_graph);
	trie_add(trie_commands, "plot", trie_encode, csfn_plot);

	trie_add(trie_commands, "modif", trie_encode, csfn_mod);
	trie_add(trie_commands, "color", trie_encode, csfn_color);
	trie_add(trie_commands, "line", trie_encode, csfn_line);

	trie_add(trie_commands, "func", trie_encode, csfn_addfunc);
	trie_add(trie_commands, "var", trie_encode, csfn_addvar);
	trie_add(trie_commands, "const", trie_encode, csfn_addconst);
	trie_add(trie_commands, "remove", trie_encode, csfn_remove);
	trie_add(trie_commands, "list", trie_encode, csfn_list);
	trie_add(trie_commands, "rename", trie_encode, csfn_rename);

	trie_add(trie_commands, "plug", trie_encode, csfn_plug);
	trie_add(trie_commands, "gccmakeplug", trie_encode, csfn_gccmakeplug);

	trie_add(trie_commands, "hide", trie_encode, csfn_hide);
	trie_add(trie_commands, "show", trie_encode, csfn_show);


	trie_colors = trie_create();

	for (size_t i = 0; i < sizeof(pallete)/sizeof(pallete[0]); i++)
		trie_add(trie_colors, pallete[i].name, trie_encode, (SDL_Color*)&pallete[i].color);

	trie_plugins = trie_create();
	/*trie_add(trie_colors,     "RED", trie_encode, &pallete[ 0]);
	trie_add(trie_colors,    "BLUE", trie_encode, &pallete[ 1]);
	trie_add(trie_colors,   "GREEN", trie_encode, &pallete[ 2]);
	trie_add(trie_colors,  "YELLOW", trie_encode, &pallete[ 3]);
	trie_add(trie_colors, "MAGENTA", trie_encode, &pallete[ 4]);
	trie_add(trie_colors,  "ORANGE", trie_encode, &pallete[ 5]);
	trie_add(trie_colors,   "BLACK", trie_encode, &pallete[ 6]);
	trie_add(trie_colors,   "WHITE", trie_encode, &pallete[ 7]);
	trie_add(trie_colors,    "GREY", trie_encode, &pallete[ 8]);
	trie_add(trie_colors,    "CYAN", trie_encode, &pallete[ 9]);
	trie_add(trie_colors,    "LIME", trie_encode, &pallete[10]);
	trie_add(trie_colors,   "BROWN", trie_encode, &pallete[11]);*/

	console((volatile _Atomic _Bool*)arg);

	return NULL;

}

void console_cleanup() {

	// No destructors needed (no malloc used)
	trie_destroy(trie_commands, NULL);
	trie_destroy(trie_colors, NULL);
	trie_destroy(trie_plugins, NULL);

	trie_commands = trie_colors = trie_plugins = NULL;

}

// = = = = THE CONSOLE PARSER = = = = 
#include <pthread.h>

// arguments are the sigquit from main, a boolean used to terminate and a mutex so rendering doesn't interfere
static void console(volatile _Atomic _Bool* sigquit) {
	system(CLEAR);
	printf("JaPlot console 0.1 - enter 'help' if you are new!\n");

	FILE* input_file = stdin;

	while (!(*sigquit)) {

		if (input_file != stdin) {
			char c;
			if (c = fgetc(input_file), feof(input_file)) {
				fclose(input_file);
				input_file = stdin;
			}
			else ungetc(c, input_file);
		}

		if (input_file == stdin)
			printf(ANSI_COLOR_YELLOW "\n > " ANSI_COLOR_RESET);
	
		// wait for command to be typed	
		char command[COMMAND_MAXLEN+1];
		fgets(command, COMMAND_MAXLEN+1, input_file);

		size_t command_len = strlen(command);	// AAn0

		if ((command_len == 0 || command[command_len-1] != '\n') && command_len < COMMAND_MAXLEN)
			strcat(command, "\n"), command_len++;

		if (command_len == COMMAND_MAXLEN) {
			printf(ANSI_COLOR_RED "Command too long! Max command length is %lu.\n" ANSI_COLOR_RESET, COMMAND_MAXLEN-2);
			continue;
		}

		if (strncmp(command, "__", 2) == 0 || command[0] == '\n') continue;

		if (command[0] == '%') {
			memmove(command, &command[1], COMMAND_MAXLEN); // shift
		} else if (input_file != stdin) printf(ANSI_COLOR_YELLOW "\n >> " ANSI_COLOR_RESET ANSI_COLOR_BLUE "%s" ANSI_COLOR_BLUE, command);

		// get the first argument (the command name)
		char *const arg = nextarg(command);

		// exit is an exception because it needs special access to the sigquit variable
		if (strcmp(arg, "exit") == 0) {
			*sigquit = 1;
			break;
		}

		// run is an exception because it needs special access to the input_file variable
		if (strcmp(arg, "run") == 0) {
			char script_name_buf[COMMAND_MAXLEN];
			char* script_name = nextarg(NULL);
			if (script_name == NULL) { printf(ANSI_COLOR_RED "Assertion failed : no script name specified\n" ANSI_COLOR_RESET); continue; }
			strcpy(script_name_buf, script_name);
			script_name = script_name_buf;

			// Append the extension if omitted
			const char* dot;
			dot = strrchr(script_name, '.');

			if (!dot || dot[1] == '\\' || dot[1] == '/')
				strcat(script_name, ".jps");

			// Open the file
			input_file = fopen(script_name, "r");
			if (!input_file) {
				printf(ANSI_COLOR_RED "File "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_RED" not found.\n" ANSI_COLOR_RESET, script_name);
				input_file = stdin;
			} else	
				printf(ANSI_COLOR_GREEN "Running "ANSI_COLOR_YELLOW"'%s'"ANSI_COLOR_GREEN"...\n\n" ANSI_COLOR_RESET, script_name);

			continue;
		}

		// process the command
		// try to find the command in the tree
		console_function csfunc = trie_find(trie_commands, arg, trie_encode);

		if (csfunc == NULL) {
			printf(ANSI_COLOR_RED "Unknown command : ["ANSI_COLOR_BLUE"%s"ANSI_COLOR_RED"]\n" ANSI_COLOR_RESET, arg);
			continue;
		} else {
			pthread_mutex_lock(&renderer_mutex);

		   	if (ERROR_FAIL(csfunc())) { // call the console function
				printf(ANSI_COLOR_RED"\nCommand ["ANSI_COLOR_BLUE"%s"ANSI_COLOR_RED"] failed,\n" ANSI_COLOR_RESET, arg);
				printf(ANSI_COLOR_RED"Enter "ANSI_COLOR_YELLOW"'help %s'"ANSI_COLOR_RED" to learn more\n", arg);
			}

			pthread_mutex_unlock(&renderer_mutex);
		}

	}
}


