#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "defs.h"
#include "decl.h"

static const char *lexer_source_path;
static int current_line;
static int current_column;
static int last_line;
static int last_column;
static int previous_line;
static int previous_column;
static int token_line;
static int token_column;

static int tracked_fgetc(FILE *infile)
{
    int c;

    previous_line = current_line;
    previous_column = current_column;
    c = fgetc(infile);
    if (c == EOF) {
        return EOF;
    }

    if (c == '\n') {
        current_line++;
        current_column = 0;
    } else {
        current_column++;
    }

    last_line = current_line;
    last_column = c == '\n' ? 1 : current_column;
    return c;
}

static void tracked_ungetc(int c, FILE *infile)
{
    if (c == EOF) {
        return;
    }
    ungetc(c, infile);
    current_line = previous_line;
    current_column = previous_column;
}

#define fgetc tracked_fgetc
#define ungetc tracked_ungetc

static void lex_error_at(int line, int column, const char *format, ...)
{
    va_list args;

    fprintf(stderr, "Lex error at %s:%d:%d: ", lexer_source_path, line, column);
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

void lex(FILE *infile, const char *source_path, struct token **tokens, int *token_count)
{
    char c;
    char buffer[256];
    int buffer_index = 0;

    lexer_source_path = source_path;
    current_line = 1;
    current_column = 0;
    last_line = 1;
    last_column = 1;

    *tokens = NULL;
    *token_count = 0;

    while ((c = fgetc(infile)) != EOF) {
        token_line = last_line;
        token_column = last_column;
        if (isspace(c)) {
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
        } else if (c == '?') {
            add_token(tokens, token_count, T_QUESTION, "?");
        } else if (c == ':') {
            add_token(tokens, token_count, T_COLON, ":");
        } else if (c == '-') {
            if ((c = fgetc(infile)) == '-') {
                add_token(tokens, token_count, T_MINUS_MINUS, "--");
            } else if (c == '=') {
                add_token(tokens, token_count, T_MINUS_ASSIGN, "-=");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_MINUS, "-");
            }
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
            if ((c = fgetc(infile)) == '+') {
                add_token(tokens, token_count, T_PLUS_PLUS, "++");
            } else if (c == '=') {
                add_token(tokens, token_count, T_PLUS_ASSIGN, "+=");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_PLUS, "+");
            }
        } else if (c == '*') {
            if ((c = fgetc(infile)) == '=') {
                add_token(tokens, token_count, T_STAR_ASSIGN, "*=");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_STAR, "*");
            }
        } else if (c == '/') {
            if ((c = fgetc(infile)) == '/') {
                while ((c = fgetc(infile)) != EOF && c != '\n') {
                }
            } else if (c == '*') {
                int prev = 0;
                int closed = 0;
                int comment_line = token_line;
                int comment_column = token_column;

                while ((c = fgetc(infile)) != EOF) {
                    if (prev == '*' && c == '/') {
                        closed = 1;
                        break;
                    }
                    prev = c;
                }

                if (!closed) {
                    lex_error_at(comment_line, comment_column, "unterminated block comment");
                }
            } else if (c == '=') {
                add_token(tokens, token_count, T_SLASH_ASSIGN, "/=");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_SLASH, "/");
            }
        } else if (c == '%') {
            if ((c = fgetc(infile)) == '=') {
                add_token(tokens, token_count, T_PERCENT_ASSIGN, "%=");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_PERCENT, "%");
            }
        } else if (c == '&') {
            if ((c = fgetc(infile)) == '&') {
                add_token(tokens, token_count, T_LOGICAL_AND, "&&");
            } else if (c == '=') {
                add_token(tokens, token_count, T_AMPERSAND_ASSIGN, "&=");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_AMPERSAND, "&");
            }
        } else if (c == '|') {
            if ((c = fgetc(infile)) == '|') {
                add_token(tokens, token_count, T_LOGICAL_OR, "||");
            } else if (c == '=') {
                add_token(tokens, token_count, T_PIPE_ASSIGN, "|=");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_PIPE, "|");
            }
        } else if (c == '^') {
            if ((c = fgetc(infile)) == '=') {
                add_token(tokens, token_count, T_CARET_ASSIGN, "^=");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_CARET, "^");
            }
        } else if (c == '=') {
            if ((c = fgetc(infile)) == '=') {
                add_token(tokens, token_count, T_EQUAL, "==");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_ASSIGN, "=");
            }
        } else if (c == '<') {
            if ((c = fgetc(infile)) == '<') {
                if ((c = fgetc(infile)) == '=') {
                    add_token(tokens, token_count, T_SHIFT_LEFT_ASSIGN, "<<=");
                } else {
                    ungetc(c, infile);
                    add_token(tokens, token_count, T_SHIFT_LEFT, "<<");
                }
            } else if (c == '=') {
                add_token(tokens, token_count, T_LESS_EQUAL, "<=");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_LESS, "<");
            }
        } else if (c == '>') {
            if ((c = fgetc(infile)) == '>') {
                if ((c = fgetc(infile)) == '=') {
                    add_token(tokens, token_count, T_SHIFT_RIGHT_ASSIGN, ">>=");
                } else {
                    ungetc(c, infile);
                    add_token(tokens, token_count, T_SHIFT_RIGHT, ">>");
                }
            } else if (c == '=') {
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

            if (strcmp(buffer, "char") == 0) {
                add_token(tokens, token_count, T_CHAR, buffer);
            } else if (strcmp(buffer, "short") == 0) {
                add_token(tokens, token_count, T_SHORT, buffer);
            } else if (strcmp(buffer, "int") == 0) {
                add_token(tokens, token_count, T_INT, buffer);
            } else if (strcmp(buffer, "long") == 0) {
                add_token(tokens, token_count, T_LONG, buffer);
            } else if (strcmp(buffer, "signed") == 0) {
                add_token(tokens, token_count, T_SIGNED, buffer);
            } else if (strcmp(buffer, "unsigned") == 0) {
                add_token(tokens, token_count, T_UNSIGNED, buffer);
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
            } else if (strcmp(buffer, "sizeof") == 0) {
                add_token(tokens, token_count, T_SIZEOF, buffer);
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
            lex_error_at(token_line, token_column, "invalid character '%c'", c);
        }
    }

    token_line = current_line;
    token_column = current_column + 1;
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
    (*tokens)[*token_count].location.line = token_line;
    (*tokens)[*token_count].location.column = token_column;
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
