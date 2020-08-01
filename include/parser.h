#pragma once

typedef struct token token;
#include "objects.h" // formula_s

#define NAME_MAXLEN 12LU

typedef struct token {
	enum {
 		TT_OPERATOR, TT_NUMBER, TT_VARIABLE, TT_FUNCTION, TT_UNKNOWN
	} type;	

    union {
        double num; // TT_NUMBER
		char name[NAME_MAXLEN]; // TT_FUNCTION or TT_CONST
        enum {
            OP_ADD = 0, OP_SUB, OP_MULT, OP_DIV, OP_MOD, OP_POW, OP_OBRACK, OP_CBRACK, OP_NEG, OP_FUNC
		} oper; // TT_OPERATOR
    };
} token;

// Converts to reverse polish notation (the variables are replaced here)
formula_s lex(const char* str);

// Computes reverse polish notation
int compute(double* result, const formula_s tokens, const double* x);

// Checks validity of a given formula
error_t validate(const formula_s tokens);
