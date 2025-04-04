#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "defs.h"

// Main lexer function that accepts a file and returns a list of tokens
void lex(FILE *infile, struct token **tokens, int *token_count) {
  char c;
  char buffer[256]; // Buffer for reading tokens
  int buffer_index = 0;
  int line = 1;

  // Initialize token list
  *tokens = NULL;
  *token_count = 0;

  while ((c = fgetc(infile)) != EOF) {
    // Skip whitespaces
    if (isspace(c)) {
      if (c == '\n') {
        line++;
      }
      continue;
    }

    // Handle braces and parentheses
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
    }
    else if (c == '-') {
      add_token(tokens, token_count, T_LOGICAL_NEGATION, "-");
    } else if (c == '~') {
        add_token(tokens, token_count, T_BITWISE_COMPLEMENT, "~");
    } else if (c == '!') {
        add_token(tokens, token_count, T_LOGICAL_NEGATION, "!");
    }
    else if (c == '+') {
      add_token(tokens, token_count, T_PLUS, "+");
    } else if (c == '*') {
        add_token(tokens, token_count, T_STAR, "*");
    } else if (c == '/') {
        add_token(tokens, token_count, T_SLASH, "/");
    } 
    // Handle keywords and identifiers
    else if (isalpha(c)) {
      buffer[buffer_index++] = c;
      while (isalnum(c = fgetc(infile)) || c == '_') {
        buffer[buffer_index++] = c;
      }
      ungetc(c, infile); // Put the last non-alphanumeric character back
      buffer[buffer_index] = '\0'; // Null-terminate the string

      if (strcmp(buffer, "int") == 0) {
        add_token(tokens, token_count, T_INT, buffer);
      } else if (strcmp(buffer, "return") == 0) {
        add_token(tokens, token_count, T_RETURN, buffer);
      } else {
        add_token(tokens, token_count, T_IDENTIFIER, buffer);
      }
      buffer_index = 0;
    }
    // Handle integer literals
    else if (isdigit(c)) {
      buffer[buffer_index++] = c;
      while (isdigit(c = fgetc(infile))) {
        buffer[buffer_index++] = c;
      }
      ungetc(c, infile); // Put the last non-digit character back
      buffer[buffer_index] = '\0'; // Null-terminate the string

      add_token(tokens, token_count, T_INTLIT, buffer);
      buffer_index = 0;
    } 
    // Invalid token
    else {
      fprintf(stderr, "Error: Invalid character '%c' on line %d\n", c, line);
      exit(EXIT_FAILURE);
    }
  }
}

// Function to add a token to the token list
void add_token(struct token **tokens, int *token_count, TokenType type, const char *value) {
  *tokens = realloc(*tokens, (*token_count + 1) * sizeof(struct token));
  (*tokens)[*token_count].type = type;
  (*tokens)[*token_count].value = strdup(value);
  (*token_count)++;
}

// Function to free allocated memory for tokens
void free_tokens(struct token *tokens, int token_count) {
  for (int i = 0; i < token_count; i++) {
    free(tokens[i].value);
  }
  free(tokens);
}
