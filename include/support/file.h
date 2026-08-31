#ifndef DONKEY_FILE_H
#define DONKEY_FILE_H

#include <stddef.h>

/*
 * Read a whole file into memory, NUL-terminated.
 *
 * Three passes had each grown their own copy of this: the lexer to get its
 * buffer, the preprocessor to read an include, and the diagnostics to quote a
 * source line.
 *
 * Returns NULL if the file cannot be read, which every caller treats as a
 * diagnostic rather than a fatal error -- a missing include names the file, and
 * a source line that cannot be quoted simply is not. *length receives the byte
 * count, not counting the terminator, and may be NULL.
 *
 * The caller owns the returned buffer.
 */
char *read_whole_file(const char *path, size_t *length);

#endif
