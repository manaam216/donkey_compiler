#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "defs.h"
#include "decl.h"
#include "diag.h"

static const char *lexer_source_path;
static int current_line;
static int current_column;
static int last_line;
static int last_column;
static int previous_line;
static int previous_column;
static int token_line;
static int token_column;

/*
 * The lexer reads from a buffer rather than a stream, so the preprocessor can
 * hand it text it produced itself -- an included file, or the result of macro
 * expansion -- with no temporary file in between.
 */
static const char *lex_text_buffer;
static size_t lex_text_length;
static size_t lex_text_position;

static int buffer_getc(void)
{
    int c;

    previous_line = current_line;
    previous_column = current_column;

    if (!lex_text_buffer || lex_text_position >= lex_text_length) {
        return EOF;
    }
    c = (unsigned char)lex_text_buffer[lex_text_position++];

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

static void buffer_ungetc(int c)
{
    if (c == EOF || lex_text_position == 0) {
        return;
    }
    lex_text_position--;
    current_line = previous_line;
    current_column = previous_column;
}

/* The lexer body is written against stdio names; point them at the buffer. */
#define fgetc(stream)     buffer_getc()
#define ungetc(c, stream) buffer_ungetc(c)

/*
 * An invalid character is reported and then skipped, so one bad byte does not
 * hide the rest of the file.
 */
static void lex_error_at(int line, int column, const char *format, ...)
{
    SourceLocation location;
    va_list args;
    char message[256];

    location.line = line;
    location.column = column;
    location.file = lexer_source_path;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    diag_at(DIAG_ERROR, location, "%s", message);
}

void lex_text(const char *text, size_t length, const char *source_path,
    struct token **tokens, int *token_count)
{
    char c;
    char buffer[256];
    int buffer_index = 0;

    lex_text_buffer = text;
    lex_text_length = length;
    lex_text_position = 0;
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
        } else if (c == '[') {
            add_token(tokens, token_count, T_OPENBRACKET, "[");
        } else if (c == ']') {
            add_token(tokens, token_count, T_CLOSEBRACKET, "]");
        } else if (c == ';') {
            add_token(tokens, token_count, T_SEMICOLON, ";");
        } else if (c == ',') {
            add_token(tokens, token_count, T_COMMA, ",");
        } else if (c == '?') {
            add_token(tokens, token_count, T_QUESTION, "?");
        } else if (c == ':') {
            add_token(tokens, token_count, T_COLON, ":");
        } else if (c == '.') {
            int second = fgetc(infile);

            if (second == '.') {
                int third = fgetc(infile);

                if (third == '.') {
                    add_token(tokens, token_count, T_ELLIPSIS, "...");
                } else {
                    ungetc(third, infile);
                    ungetc(second, infile);
                    add_token(tokens, token_count, T_DOT, ".");
                }
            } else {
                ungetc(second, infile);
                add_token(tokens, token_count, T_DOT, ".");
            }
        } else if (c == '\'') {
            int value = 0;
            c = fgetc(infile);
            if (c == EOF || c == '\n') {
                lex_error_at(token_line, token_column, "unterminated character literal");
                /* Hand the newline back so line counting stays right. */
                if (c == '\n') {
                    ungetc(c, infile);
                }
                add_token(tokens, token_count, T_CHARLIT, "0");
                continue;
            }
            if (c == '\\') {
                c = fgetc(infile);
                if (c == 'n') value = '\n';
                else if (c == '0') value = '\0';
                else if (c == '\'') value = '\'';
                else if (c == '\\') value = '\\';
                else lex_error_at(token_line, token_column, "unsupported character escape '\\%c'", c);
            } else {
                value = c;
            }
            c = fgetc(infile);
            if (c != '\'') {
                lex_error_at(token_line, token_column, "unterminated character literal");
                if (c == '\n' || c == EOF) {
                    if (c == '\n') {
                        ungetc(c, infile);
                    }
                    add_token(tokens, token_count, T_CHARLIT, "0");
                    continue;
                }
            }
            snprintf(buffer, sizeof(buffer), "%d", value);
            add_token(tokens, token_count, T_CHARLIT, buffer);
        } else if (c == '"') {
            buffer_index = 0;
            while ((c = fgetc(infile)) != EOF && c != '"') {
                if (c == '\n') {
                    lex_error_at(token_line, token_column, "unterminated string literal");
                }
                if (c == '\\') {
                    c = fgetc(infile);
                    if (c == 'n') c = '\n';
                    else if (c == '0') c = '\0';
                    else if (c == '"') c = '"';
                    else if (c == '\\') c = '\\';
                    else lex_error_at(token_line, token_column, "unsupported string escape '\\%c'", c);
                }
                if (buffer_index >= (int)sizeof(buffer) - 1) {
                    /*
                     * Report once, then keep scanning for the closing quote
                     * without storing more. Appending here would overrun the
                     * buffer now that an error no longer ends compilation.
                     */
                    if (buffer_index == (int)sizeof(buffer) - 1) {
                        lex_error_at(token_line, token_column,
                            "string literal is too long");
                        buffer_index++;
                    }
                    continue;
                }
                buffer[buffer_index++] = c;
            }
            if (c != '"') {
                lex_error_at(token_line, token_column, "unterminated string literal");
            }
            if (buffer_index > (int)sizeof(buffer) - 1) {
                buffer_index = (int)sizeof(buffer) - 1;   /* truncated above */
            }
            buffer[buffer_index] = '\0';
            add_token(tokens, token_count, T_STRINGLIT, buffer);
            buffer_index = 0;
        } else if (c == '-') {
            if ((c = fgetc(infile)) == '-') {
                add_token(tokens, token_count, T_MINUS_MINUS, "--");
            } else if (c == '>') {
                add_token(tokens, token_count, T_ARROW, "->");
            } else if (c == '=') {
                add_token(tokens, token_count, T_MINUS_ASSIGN, "-=");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_MINUS, "-");
            }
        } else if (c == '#') {
            if ((c = fgetc(infile)) == '#') {
                add_token(tokens, token_count, T_HASH_HASH, "##");
            } else {
                ungetc(c, infile);
                add_token(tokens, token_count, T_HASH, "#");
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
            } else if (strcmp(buffer, "void") == 0) {
                add_token(tokens, token_count, T_VOID, buffer);
            } else if (strcmp(buffer, "union") == 0) {
                add_token(tokens, token_count, T_UNION, buffer);
            } else if (strcmp(buffer, "enum") == 0) {
                add_token(tokens, token_count, T_ENUM, buffer);
            } else if (strcmp(buffer, "typedef") == 0) {
                add_token(tokens, token_count, T_TYPEDEF, buffer);
            } else if (strcmp(buffer, "extern") == 0) {
                add_token(tokens, token_count, T_EXTERN, buffer);
            } else if (strcmp(buffer, "static") == 0) {
                add_token(tokens, token_count, T_STATIC, buffer);
            } else if (strcmp(buffer, "const") == 0) {
                add_token(tokens, token_count, T_CONST, buffer);
            } else if (strcmp(buffer, "volatile") == 0) {
                add_token(tokens, token_count, T_VOLATILE, buffer);
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
            } else if (strcmp(buffer, "do") == 0) {
                add_token(tokens, token_count, T_DO, buffer);
            } else if (strcmp(buffer, "switch") == 0) {
                add_token(tokens, token_count, T_SWITCH, buffer);
            } else if (strcmp(buffer, "case") == 0) {
                add_token(tokens, token_count, T_CASE, buffer);
            } else if (strcmp(buffer, "default") == 0) {
                add_token(tokens, token_count, T_DEFAULT, buffer);
            } else if (strcmp(buffer, "goto") == 0) {
                add_token(tokens, token_count, T_GOTO, buffer);
            } else if (strcmp(buffer, "for") == 0) {
                add_token(tokens, token_count, T_FOR, buffer);
            } else if (strcmp(buffer, "break") == 0) {
                add_token(tokens, token_count, T_BREAK, buffer);
            } else if (strcmp(buffer, "continue") == 0) {
                add_token(tokens, token_count, T_CONTINUE, buffer);
            } else if (strcmp(buffer, "struct") == 0) {
                add_token(tokens, token_count, T_STRUCT, buffer);
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
    (*tokens)[*token_count].location.file = lexer_source_path;
    (*tokens)[*token_count].value = strdup(value);
    /*
     * The preprocessor needs to know where lines begin, and the token stream
     * has no newlines in it. Recording it here is the cheapest place: a token
     * starts a line if nothing has been emitted on that line yet.
     */
    (*tokens)[*token_count].at_line_start =
        *token_count == 0 || (*tokens)[*token_count - 1].location.line != token_line;
    (*token_count)++;
}

void free_tokens(struct token *tokens, int token_count)
{
    for (int i = 0; i < token_count; i++) {
        free(tokens[i].value);
    }
    free(tokens);
}

/*
 * Convenience wrapper: slurp the stream, then lex it. The buffer form is what
 * the preprocessor uses.
 */
void lex(FILE *infile, const char *source_path, struct token **tokens, int *token_count)
{
    char *text;
    long size;
    size_t read;

    *tokens = NULL;
    *token_count = 0;

    if (fseek(infile, 0, SEEK_END) != 0 || (size = ftell(infile)) < 0) {
        return;
    }
    rewind(infile);

    text = malloc((size_t)size + 1);
    if (!text) {
        fprintf(stderr, "Out of memory reading %s\n", source_path);
        exit(EXIT_FAILURE);
    }
    read = fread(text, 1, (size_t)size, infile);
    text[read] = '\0';

    lex_text(text, read, source_path, tokens, token_count);
    free(text);
}
