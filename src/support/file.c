#include <stdio.h>
#include <stdlib.h>
#include "support/file.h"
#include "support/mem.h"

char *read_whole_file(const char *path, size_t *length)
{
    FILE *file;
    char *text;
    long size;
    size_t read;

    if (length) {
        *length = 0;
    }

    /*
     * Binary mode: a newline must stay one byte. Reading in text mode on
     * Windows would collapse CRLF, and the positions every diagnostic points
     * at would no longer match the bytes on disk.
     */
    file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0) {
        fclose(file);
        return NULL;
    }
    rewind(file);

    text = xmalloc((size_t)size + 1, "file contents");
    read = fread(text, 1, (size_t)size, file);
    text[read] = '\0';
    fclose(file);

    if (length) {
        *length = read;
    }
    return text;
}
