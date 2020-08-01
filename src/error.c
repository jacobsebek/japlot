#include "error.h"

// --------- ERROR THROWING/CATCHIN' STUFF-----------
static char error[100];

void error_throw(const char* msg) {
	snprintf(error, 100, msg);
}

void error_throw_str(const char* msg, const char* str) {
	snprintf(error, 100, msg, str);
}

void error_throw_val(const char* msg, const long val) {
	snprintf(error, 100, msg, val);
}

const char* error_catch() {
	return error;	
}
