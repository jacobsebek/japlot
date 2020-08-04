#include "parser.h"
#include "error.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#define STACK_POP(ts) (*(--(ts).stacktop))
#define STACK_PUSH(ts, val) (*((ts).stacktop++)) = val
#define STACK_PEEK(ts) ((ts).stacktop-1)
#define STACK_HEIGHT(ts) ((ts).stacktop - (ts).stackbot)
#define STACK_ALLOC(ts, length, type) (ts).stackbot = malloc(length*sizeof(type)); (ts).stacktop = (ts).stackbot
#define STACK_FREE(ts) free((ts).stackbot)

#define ISNUMBER(c) (isdigit(c) || c == '.')
#define ISSIGN(c) (!isalnum(c) && c != '.')

static const int precedence[] = {0,0,1,1,1,2,-1,-1, 0}; // the precedence of the enumeration operators (defined in header)

typedef struct tokenstack {
    token* stackbot;
    token* stacktop;
} tokenstack;

typedef struct numberstack {
    double* stackbot;
    double* stacktop;
} numberstack;

//This is just for convenience
static void token_set_operator(token* tok, int oper) {
    tok->type = TT_OPERATOR;
    tok->oper = oper;
}

// Converts to reverse polish notation using the shunting yard algorithm
// https://en.wikipedia.org/wiki/Shunting-yard_algorithm
formula_s lex(const char* str) {
    if (str == NULL || strlen(str) == 0) {
        error_throw("no input specified");
        return (formula_s){NULL, 0};
    }

    // first, tokenize the string
    /*
    0. return the token list if the current character is '\0'
    1. skip all the following spaces
    2. Determine the token type base on its first character
    3. Add characters to the buffer until there is a character
       that does not correspond the current token type
    4. parse the current token and put it into the token list
    5. goto step 0
    */

    size_t numtoks = 0; // numtoks - total number of tokens, numopers - number of operators (!isnumber)
    token* tokens = NULL;

    int depth = 0;

    while (1) {
            while (isblank(*str)) str++; // skip all the following spaces
            if (*str == '\0') break;

            char currstr[NAME_MAXLEN];
            char* currchar = &currstr[0];
            _Bool isnumber = ISNUMBER(*str); // Determine the token type base on its first character
        _Bool isletter = isalpha(*str);

            // Add characters to the buffer until there is a character that does not match the current token type, whitespace character or EOS
            while ((isnumber ? ISNUMBER(*str) : isletter ? (isalnum(*str) || *str == '_') : (!ISNUMBER(*str) && !isalpha(*str)))
                && *str != '\0' && !isspace(*str)) {

                    *(currchar++) = *(str++);

                    // brackets are an exception where two operators can be next to each other, e.g. *(())
                    if (*(str) == '(' || *(str-1) == '(' || *(str-1) == ')') break;
            }

            *currchar = '\0';
            if (strlen(currstr) == 0) break;

        if  (currstr[0] == '(') depth++;
        else if (currstr[0] == ')') depth--;

            // create the token
            tokens = realloc(tokens, (++numtoks)*sizeof(token));
        token *currtok = &tokens[numtoks-1], 
              *prevtok = numtoks > 1 ? &tokens[numtoks-2] : NULL;

        // parse the current token

            if (isnumber) {
            currtok->type = TT_NUMBER;
            currtok->num = atof(currstr);
            } 
        else if (strcmp(currstr, "+") == 0) token_set_operator(currtok, OP_ADD);
        else if (strcmp(currstr, "-") == 0) {
            // decide if it is the minus operator or the negation sign
            if (prevtok == NULL || (prevtok->type == TT_OPERATOR && prevtok->oper == OP_OBRACK))
                token_set_operator(currtok, OP_NEG);
            else 
                token_set_operator(currtok, OP_SUB);
        }
        else if (strcmp(currstr, "*") == 0) token_set_operator(currtok, OP_MULT);
        else if (strcmp(currstr, "/") == 0) token_set_operator(currtok, OP_DIV);
        else if (strcmp(currstr, "^") == 0) token_set_operator(currtok, OP_POW);
        else if (strcmp(currstr, "(") == 0) token_set_operator(currtok, OP_OBRACK);
        else if (strcmp(currstr, ")") == 0) token_set_operator(currtok, OP_CBRACK);
        else if (strcmp(currstr, "mod") == 0) token_set_operator(currtok, OP_MOD);
        else {
            currtok->type = TT_UNKNOWN;
            strcpy(currtok->name, currstr);
        }

        // This decides if a keyword is a function or a variable
        // e.g. in "cos 2" "cos" is a function, that is not the case in "cos + 2"
        if (prevtok != NULL && prevtok->type == TT_UNKNOWN) {
            if (currtok->type == TT_OPERATOR && currtok->oper == OP_OBRACK)
                prevtok->type = TT_FUNCTION;
            else
                prevtok->type = TT_VARIABLE;
        }

        // This allow sytactic sugar like 2x instead of 2*x (it just squeezes the * in between)
        if (prevtok != NULL && prevtok->type == TT_NUMBER && (currtok->type == TT_UNKNOWN || (currtok->type == TT_OPERATOR && currtok->oper == OP_OBRACK))) {
            tokens = realloc(tokens, (++numtoks)*sizeof(token));
            tokens[numtoks-1] = tokens[numtoks-2];
            token_set_operator(&tokens[numtoks-2], OP_MULT);
        }

        }

    if (numtoks > 0 && tokens[numtoks-1].type == TT_UNKNOWN)
        tokens[numtoks-1].type = TT_VARIABLE;

    if (depth != 0) {
        error_throw("bracket error");
        free(tokens);
        return (formula_s){NULL, 0}; 
    }

    // Now, convert the infix notation to postifix (reverse polish) using the shunting yard algorithm

    // initialize the operator stack
    tokenstack oper_stack;
    STACK_ALLOC(oper_stack, numtoks, token);

    // this is the final array
    tokenstack tokens_postfix;
    STACK_ALLOC(tokens_postfix, numtoks, token);

    for (size_t i = 0; i < numtoks; i++) {
    if (tokens[i].type == TT_NUMBER || tokens[i].type == TT_VARIABLE)
            STACK_PUSH(tokens_postfix, tokens[i]);
    else if (tokens[i].type == TT_OPERATOR) {

            if (tokens[i].oper == OP_CBRACK) {

                while (STACK_HEIGHT(oper_stack) > 0 && (oper_stack.stacktop-1)->oper != OP_OBRACK)
                    STACK_PUSH(tokens_postfix, STACK_POP(oper_stack));

                STACK_POP(oper_stack); // Get rid of the unwanted opening bracket that is just left over now
            } else {

                if (tokens[i].oper != OP_OBRACK) // we dont want the opening bracket to pop ANYTHING
                    while (STACK_HEIGHT(oper_stack) > 0 && precedence[tokens[i].oper] <= (STACK_PEEK(oper_stack)->type == TT_FUNCTION ? 3 : precedence[STACK_PEEK(oper_stack)->oper]))
                        STACK_PUSH(tokens_postfix, STACK_POP(oper_stack));

                STACK_PUSH(oper_stack, tokens[i]);
            }
        } else if (tokens[i].type == TT_FUNCTION) {

            while (STACK_HEIGHT(oper_stack) > 0 && 3 <= (STACK_PEEK(oper_stack)->type == TT_FUNCTION ? 3 : precedence[STACK_PEEK(oper_stack)->oper]))
                STACK_PUSH(tokens_postfix, STACK_POP(oper_stack));

            STACK_PUSH(oper_stack, tokens[i]);

        }

    }

    // pop off the whole stack before finishing
    while(STACK_HEIGHT(oper_stack) > 0) 
        STACK_PUSH(tokens_postfix, STACK_POP(oper_stack));

    free(tokens);
    STACK_FREE(oper_stack);

    // construct the result structure
    formula_s result = {tokens_postfix.stackbot, STACK_HEIGHT(tokens_postfix)};
    return result;
}

// Computes reverse polish notation
// https://en.wikipedia.org/wiki/Reverse_Polish_notation
int compute(double* result, const formula_s formula, const double* x) {
    if (formula.toks == NULL) {
        error_throw("invalid formula");
        return 0;
    }

    numberstack numstack;
    STACK_ALLOC(numstack, formula.numtoks, double);

    for (size_t i = 0; i < formula.numtoks; i++) {
        
        if (formula.toks[i].type == TT_NUMBER) {
            STACK_PUSH(numstack, formula.toks[i].num);
        } else if (formula.toks[i].type == TT_OPERATOR){
            if (STACK_HEIGHT(numstack) < (formula.toks[i].oper == OP_NEG ? 1 : 2)) {
                error_throw("insufficent operand count");
                return 0;
            }
            
            double right = STACK_POP(numstack);
            double left;

            if (formula.toks[i].oper != OP_NEG)
                left = STACK_POP(numstack);

            switch (formula.toks[i].oper) {
                case OP_ADD : STACK_PUSH(numstack, left + right); break;
                case OP_SUB : STACK_PUSH(numstack, left - right); break;
                case OP_MULT: STACK_PUSH(numstack, left * right); break;
                case OP_DIV : STACK_PUSH(numstack, left / right); break;
                case OP_MOD : STACK_PUSH(numstack, fmod(left, right)); break;
                case OP_POW : STACK_PUSH(numstack, pow(left, right)); break;
                case OP_NEG : STACK_PUSH(numstack, -right); break; // negate the top of the stack 
            }

        } else {

            double pushval;

            if (x != NULL && !strcmp(formula.toks[i].name, "x"))
                pushval = *x;
            else {

                object* obj;
                if (ERROR_FAIL(object_get(formula.toks[i].name, &obj))) 
                    return ERROR_CODE_FAIL;

                switch(formula.toks[i].type) {
                    case TT_VARIABLE :
                        if (obj->type != OT_VARIABLE && obj->type != OT_CONSTANT) {
                            error_throw_str("%s is not a variable", formula.toks[i].name);
                            return ERROR_CODE_FAIL;
                        }

                        pushval = *obj->val;
                    break;
                    case TT_FUNCTION :
                        if (STACK_HEIGHT(numstack) < 1) {
                            error_throw("function argument missing");
                            return ERROR_CODE_FAIL;
                        }

                        if (obj->type == OT_CFUNC)
                            pushval = obj->cfunc(STACK_POP(numstack));
                        else if (obj->type == OT_FUNCTION) {
                            if (ERROR_FAIL(compute(&pushval, *obj->func, &STACK_POP(numstack)))) 
                                return ERROR_CODE_FAIL;
                        } else {
                            error_throw_str("%s is not a function", formula.toks[i].name);
                            return ERROR_CODE_FAIL;
                        }

                    break;
                    default : 
                        error_throw("unknown type token"); 
                        return ERROR_CODE_FAIL;
                    break;
                }
            }

            STACK_PUSH(numstack, pushval);

        }

    }

    if (STACK_HEIGHT(numstack) > 1) {
        error_throw("insufficent operator count");
        return ERROR_CODE_FAIL;
    }

    if (result != NULL) 
        *result = STACK_POP(numstack);

    STACK_FREE(numstack);

    return ERROR_CODE_OK;
}

error_t validate(const formula_s tokens) {
    return compute(NULL, tokens, &(double){0});
}
