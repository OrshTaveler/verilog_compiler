#include <stddef.h>


typedef struct {
    char** data;    
    size_t size;    
    size_t capacity; 
} StringArray;

StringArray** read_and_split(char* input, int* len);
char* read_entire_file(const char* filename, unsigned int* len);