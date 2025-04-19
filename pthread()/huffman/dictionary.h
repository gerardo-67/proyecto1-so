#include <stdio.h>

#ifndef DICTIONARY_H
#define DICTIONARY_H

#define MAX_SIZE 1920

typedef struct {
    char key;  
    int value;   
} Dictionary;

void put(Dictionary dictionary[], int* size, char key, int value) {
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

#endif