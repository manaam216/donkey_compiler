/*
 * Lexer tests run in process, so they can check things the generated assembly
 * never shows: exact token kinds, and the line and column each one carries.
 * Those positions are what every diagnostic points at, so they are worth
 * asserting directly.
 */
#include <stdio.h>
#include <stdlib.h>
#include "defs.h"
#include "decl.h"
#include "diag.h"
#include "unit.h"

static struct token *tokens;
static int token_count;

static void lex_source(const char *text)
{
    FILE *file = tmpfile();

    if (!file) {
        printf("FAIL: could not create a temporary file\n");
        unit_failures++;
        return;
    }

    fputs(text, file);
    rewind(file);

    free_tokens(tokens, token_count);
    tokens = NULL;
    token_count = 0;

    diag_init("<test>");
    lex(file, "<test>", &tokens, &token_count);
    fclose(file);
}

static void check_token(int index, TokenType type, const char *value,
    int line, int column)
{
    char label[64];

    if (index >= token_count) {
        printf("FAIL: expected a token at index %d, only got %d\n",
            index, token_count);
        unit_failures++;
        return;
    }

    snprintf(label, sizeof(label), "token %d kind", index);
    check_int(label, tokens[index].type, type);

    if (value) {
        snprintf(label, sizeof(label), "token %d text", index);
        check_str(label, tokens[index].value, value);
    }

    snprintf(label, sizeof(label), "token %d line", index);
    check_int(label, tokens[index].location.line, line);
    snprintf(label, sizeof(label), "token %d column", index);
    check_int(label, tokens[index].location.column, column);
}

static void test_positions(void)
{
    lex_source("int x;\nx = 42;\n");

    check_token(0, T_INT, "int", 1, 1);
    check_token(1, T_IDENTIFIER, "x", 1, 5);
    check_token(2, T_SEMICOLON, ";", 1, 6);
    check_token(3, T_IDENTIFIER, "x", 2, 1);
    check_token(4, T_ASSIGN, "=", 2, 3);
    check_token(5, T_INTLIT, "42", 2, 5);
}

static void test_multi_character_operators(void)
{
    lex_source("a <<= b >> c && d");

    check_int("<<= is one token", tokens[1].type, T_SHIFT_LEFT_ASSIGN);
    check_int(">> is one token", tokens[3].type, T_SHIFT_RIGHT);
    check_int("&& is one token", tokens[5].type, T_LOGICAL_AND);
}

static void test_comments_are_skipped(void)
{
    lex_source("int /* block */ x; // trailing\nint y;\n");

    check_int("comment produces no token", tokens[1].type, T_IDENTIFIER);
    /* The line comment must not swallow the newline after it. */
    check_token(4, T_IDENTIFIER, "y", 2, 5);
}

static void test_character_escapes(void)
{
    lex_source("'A' '\\n' '\\0'");

    check_str("'A'", tokens[0].value, "65");
    check_str("newline escape", tokens[1].value, "10");
    check_str("nul escape", tokens[2].value, "0");
}

static void test_recovers_from_invalid_character(void)
{
    int errors_before;

    diag_init("<test>");
    errors_before = diag_error_count();
    lex_source("int @ x;");

    /*
     * An invalid character is reported, then skipped: the tokens on either
     * side of it must still be produced.
     */
    check_int("invalid character reported", diag_error_count() > errors_before, 1);
    check_int("first token survives", tokens[0].type, T_INT);
    check_str("token after the bad character survives", tokens[1].value, "x");
}

int main(void)
{
    test_positions();
    test_multi_character_operators();
    test_comments_are_skipped();
    test_character_escapes();
    test_recovers_from_invalid_character();

    free_tokens(tokens, token_count);
    diag_cleanup();
    return unit_report("lexer");
}
