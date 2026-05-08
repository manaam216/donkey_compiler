#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "defs.h"
#include "decl.h"

void lex(FILE *infile, struct token **tokens, int *token_count)
{
    char c;
    char buffer[256];
    int buffer_index = 0;
    int line = 1;

    *tokens = NULL;
    *token_count = 0;

    while ((c = fgetc(infile)) != EOF) {
        if (isspace(c)) {
            if (c == '\n') {
                line++;
            }
            continue;
        }

        if (c == '{') {
            add_token(tokens, token_count, T_OPENBRACE, "{");
        } else if (c == '}') {
            add_token(tokens, token_count, T_CLOSEBRACE, "}");
        } else if (c == '(') {
            add_token(tokens, token_count, T_OPENPAREN, "(");
        } else if (c == ')') {
            add_token(tokens, token_count, T_CLOSEPAREN, ")");
        } else if (c == ';') {
            add_token(tokens, token_count, T_SEMICOLON, ";");
        } else if (c == ',') {
            add_token(tokens, token_count, T_COMMA, ",");
        } else if (c == '-') {
            add_token(tokens, token_count, T_MINUS, "-");
        } else if (c == '~') {
            add_token(tokens, token_count, T_BITWISE_COMPLEMENT, "~");
        } else if (c == '!') {
            if ((c = fgetc(infile)) == '=') {
                add_token(tokens, token_count, T_NOT_EQUAL, "!=");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_LOGICAL_NEGATION, "!");
            }
        } else if (c == '+') {
            add_token(tokens, token_count, T_PLUS, "+");
        } else if (c == '*') {
            add_token(tokens, token_count, T_STAR, "*");
        } else if (c == '/') {
            add_token(tokens, token_count, T_SLASH, "/");
        } else if (c == '%') {
            add_token(tokens, token_count, T_PERCENT, "%");
        } else if (c == '&') {
            if ((c = fgetc(infile)) == '&') {
                add_token(tokens, token_count, T_LOGICAL_AND, "&&");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_AMPERSAND, "&");
            }
        } else if (c == '|') {
            if ((c = fgetc(infile)) == '|') {
                add_token(tokens, token_count, T_LOGICAL_OR, "||");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_PIPE, "|");
            }
        } else if (c == '^') {
            add_token(tokens, token_count, T_CARET, "^");
        } else if (c == '=') {
            if ((c = fgetc(infile)) == '=') {
                add_token(tokens, token_count, T_EQUAL, "==");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_ASSIGN, "=");
            }
        } else if (c == '<') {
            if ((c = fgetc(infile)) == '=') {
                add_token(tokens, token_count, T_LESS_EQUAL, "<=");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_LESS, "<");
            }
        } else if (c == '>') {
            if ((c = fgetc(infile)) == '=') {
                add_token(tokens, token_count, T_GREATER_EQUAL, ">=");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_GREATER, ">");
            }
        } else if (isalpha(c)) {
            buffer[buffer_index++] = c;
            while (isalnum(c = fgetc(infile)) || c == '_') {
                buffer[buffer_index++] = c;
            }
            ungetc(c, infile);
            buffer[buffer_index] = '\0';

            if (strcmp(buffer, "int") == 0) {
                add_token(tokens, token_count, T_INT, buffer);
            } else if (strcmp(buffer, "return") == 0) {
                add_token(tokens, token_count, T_RETURN, buffer);
            } else if (strcmp(buffer, "if") == 0) {
                add_token(tokens, token_count, T_IF, buffer);
            } else if (strcmp(buffer, "else") == 0) {
                add_token(tokens, token_count, T_ELSE, buffer);
            } else if (strcmp(buffer, "while") == 0) {
                add_token(tokens, token_count, T_WHILE, buffer);
            } else if (strcmp(buffer, "for") == 0) {
                add_token(tokens, token_count, T_FOR, buffer);
            } else if (strcmp(buffer, "break") == 0) {
                add_token(tokens, token_count, T_BREAK, buffer);
            } else if (strcmp(buffer, "continue") == 0) {
                add_token(tokens, token_count, T_CONTINUE, buffer);
            } else {
                add_token(tokens, token_count, T_IDENTIFIER, buffer);
            }
            buffer_index = 0;
        } else if (isdigit(c)) {
            buffer[buffer_index++] = c;
            while (isdigit(c = fgetc(infile))) {
                buffer[buffer_index++] = c;
            }
            ungetc(c, infile);
            buffer[buffer_index] = '\0';

            add_token(tokens, token_count, T_INTLIT, buffer);
            buffer_index = 0;
        } else {
            fprintf(stderr, "Error: Invalid character '%c' on line %d\n", c, line);
            exit(EXIT_FAILURE);
        }
    }

    add_token(tokens, token_count, T_EOF, "EOF");
}

void add_token(struct token **tokens, int *token_count, TokenType type, const char *value)
{
    *tokens = realloc(*tokens, (*token_count + 1) * sizeof(struct token));
    if (!*tokens) {
        perror("Error allocating token");
        exit(EXIT_FAILURE);
    }

    (*tokens)[*token_count].type = type;
    (*tokens)[*token_count].value = strdup(value);
    (*token_count)++;
}

void free_tokens(struct token *tokens, int token_count)
{
    for (int i = 0; i < token_count; i++) {
        free(tokens[i].value);
    }
    free(tokens);
}
