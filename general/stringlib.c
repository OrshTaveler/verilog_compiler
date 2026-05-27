#include "general.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* read_entire_file(const char* filename, unsigned int* len) {
    FILE* f = fopen(filename, "r");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }
    
    fread(content, 1, size, f);
    content[size] = '\0';
    
    fclose(f);

    if (len) {
        *len = (unsigned int)size;
    }
    
    return content;
}


StringArray** read_and_split(char* input, int* len) {
    int array_capacity = 16;
    StringArray** array = malloc(array_capacity * sizeof(StringArray*));

    int char_capacity = 16;
    int tokens = 0;
    int i = 0;
    int clen = 0;
    int lines = 0;

    char* c = malloc(char_capacity);

    array[lines] = malloc(sizeof(StringArray));
    array[lines]->data = malloc(16 * sizeof(char*));

    while (input[i] != '\0') {

        if (clen >= char_capacity - 1) {
            char_capacity += 16;
            c = realloc(c, char_capacity);
        }

        if (input[i] == ' ' || input[i] == '\n') {

            if (clen > 0) {
                c[clen] = '\0';

                array[lines]->data =
                    realloc(array[lines]->data, (tokens + 1) * sizeof(char*));

                array[lines]->data[tokens++] = strdup(c);
                clen = 0;
            }

            if (input[i] == '\n') {
                array[lines]->size = tokens;
                array[lines]->capacity = tokens;

                lines++;

                if (lines >= array_capacity) {
                    array_capacity += 16;
                    array = realloc(array, array_capacity * sizeof(StringArray*));
                }

                tokens = 0;

                array[lines] = malloc(sizeof(StringArray));
                array[lines]->data = malloc(16 * sizeof(char*));
            }

            i++;
        } else {
            c[clen++] = input[i++];
        }
    }

    if (clen > 0) {
        c[clen] = '\0';
        array[lines]->data =
            realloc(array[lines]->data, (tokens + 1) * sizeof(char*));
        array[lines]->data[tokens++] = strdup(c);
    }

    array[lines]->size = tokens;
    array[lines]->capacity = tokens;
    *len = lines + 1;

    free(c);
    return array;
}
