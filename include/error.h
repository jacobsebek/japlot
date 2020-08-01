#pragma once

#include <stdio.h>
#define EXCEPT(msg) { fputs(msg"\n", stderr); exit(-1); }

#define ERROR_CODE_FAIL 0
#define ERROR_CODE_OK 1

#define ERROR_MSG(errtype) printf("\x1b[91mAn error occured whilst "errtype" \x1b[0m: %s\n", error_catch());
#define ERROR_FAIL(val) (val == ERROR_CODE_FAIL)

typedef int error_t;

void error_throw(const char* msg);
void error_throw_str(const char* msg, const char* str);
void error_throw_val(const char* msg, const long val);

const char* error_catch();
