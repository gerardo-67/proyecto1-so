#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// DICTIONARY_H
#define MAX_SIZE 1920

typedef struct {
    char key;  
    int value;   
} Dictionary;

void put_dictionary(Dictionary dictionary[], int* size, char key, int value) {
    if (*size >= MAX_SIZE) {  
        return;
    }
    // Reemplazar el valor si la key ya existe
    for (int i = 0; i < *size; i++) {
        if (dictionary[i].key == key) {
            dictionary[i].value = value;
            return;  
        }
    }
    dictionary[*size].key = key;
    dictionary[*size].value = value;
    (*size)++;  // Incrementar el tamaño del diccionario
}

int get(Dictionary dictionary[], int size, char key) {
    for (int i = 0; i < size; i++) {
        if (dictionary[i].key == key) {
            return dictionary[i].value;
        }
    }
    return -1; // Key not found
}

// En esta función se incrementa el valor de la key en 1
// si ya existe, o se añade una nueva key con valor 1
void put_2_dictionary(Dictionary dictionary[], int* size, char key) {
    if (*size >= MAX_SIZE) {  
        return;
    }
    // Reemplazar el valor si la key ya existe
    for (int i = 0; i < *size; i++) {
        if (dictionary[i].key == key) {
            dictionary[i].value += 1;
            return;  
        }
    }
    dictionary[*size].key = key;
    dictionary[*size].value = 1;
    (*size)++;  // Incrementar el tamaño del diccionario
}

void print_dictionary(Dictionary dictionary[], int size) {
    for (int i = 0; i < size; i++) {
        printf("Key: %c, Value: %d\n", dictionary[i].key, dictionary[i].value);
    }
}

// END_DICTIONARY_H

char* get_file_content(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        return NULL; // File not found
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* content = (char*)malloc(length + 1);
    if (content == NULL) {
        fclose(file);
        return NULL; // Memory allocation failed
    }
    fread(content, 1, length, file);
    content[length] = '\0'; // Null-terminate the string
    fclose(file);
    return content; // Return the file content
}

int main (){
    const char* filename = "text.txt";
    char* content = get_file_content(filename);

    Dictionary dictionary[MAX_SIZE];
    int size = 0;

    for (int i = 0; content[i] != '\0'; i++) {
        put_2_dictionary(dictionary, &size, content[i]);
    }
    print_dictionary(dictionary, size);
    free(content); // Free the allocated memory for file content
    return 0;
}