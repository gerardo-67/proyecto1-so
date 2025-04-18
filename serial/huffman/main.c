#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "node.h"
#include "dictionary.h"

#define MAX_SIZE 1920
#define MAX_CODE_LENGTH 256

typedef struct {
    char character;
    char code[MAX_CODE_LENGTH];
} HuffmanCode;

Node** crear_nodos_desde_diccionario(Dictionary diccionario[], int tamaño) {
    Node** nodos = malloc(tamaño * sizeof(Node*));
    if (!nodos) return NULL;

    for (int i = 0; i < tamaño; i++) {
        Node* nuevo = malloc(sizeof(Node));
        if (!nuevo) return NULL;
        nuevo->character = diccionario[i].key;
        nuevo->frequency = diccionario[i].value;
        nuevo->left = NULL;
        nuevo->right = NULL;
        nodos[i] = nuevo;
    }
    return nodos;
}

int comparar_nodos(const void* a, const void* b) {
    Node* nodoA = *(Node**)a;
    Node* nodoB = *(Node**)b;
    return nodoA->frequency - nodoB->frequency;
}

Node* construir_arbol_huffman(Node** nodos, int cantidad) {
    while (cantidad > 1) {
        qsort(nodos, cantidad, sizeof(Node*), comparar_nodos);

        Node* izquierda = nodos[0];
        Node* derecha = nodos[1];

        Node* nuevo = malloc(sizeof(Node));
        if (!nuevo) return NULL;

        nuevo->character = '\0';
        nuevo->frequency = izquierda->frequency + derecha->frequency;
        nuevo->left = izquierda;
        nuevo->right = derecha;

        nodos[0] = nuevo;
        nodos[1] = nodos[cantidad - 1];
        cantidad--;
    }

    return nodos[0];
}

void generar_codigos(Node* raiz, char* codigo_actual, int profundidad, HuffmanCode* tabla, int* indice) {
    if (!raiz) return;

    if (!raiz->left && !raiz->right) {
        tabla[*indice].character = raiz->character;
        codigo_actual[profundidad] = '\0';
        strcpy(tabla[*indice].code, codigo_actual);
        (*indice)++;
        return;
    }

    codigo_actual[profundidad] = '0';
    generar_codigos(raiz->left, codigo_actual, profundidad + 1, tabla, indice);
    codigo_actual[profundidad] = '1';
    generar_codigos(raiz->right, codigo_actual, profundidad + 1, tabla, indice);
}

const char* obtener_codigo(HuffmanCode* tabla, int total, char caracter) {
    for (int i = 0; i < total; i++) {
        if (tabla[i].character == caracter) {
            return tabla[i].code;
        }
    }
    return NULL;
}

void liberar_arbol(Node* nodo) {
    if (!nodo) return;
    liberar_arbol(nodo->left);
    liberar_arbol(nodo->right);
    free(nodo);
}

void comprimir_archivo(FILE* salida, const char* ruta_completa, const char* nombre_archivo) {
    FILE* f = fopen(ruta_completa, "r");
    if (!f) {
        printf("No se pudo abrir %s\n", ruta_completa);
        return;
    }

    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (tam <= 0) {
        printf("Archivo vacío: %s — se omite\n", nombre_archivo);
        fclose(f);
        return;
    }

    char* contenido = malloc(tam + 1);
    if (!contenido) {
        printf("Error asignando memoria para %s\n", nombre_archivo);
        fclose(f);
        return;
    }

    fread(contenido, 1, tam, f);
    contenido[tam] = '\0';
    fclose(f);

    Dictionary diccionario[MAX_SIZE];
    int tam_dic = 0;
    for (int i = 0; contenido[i] != '\0'; i++) {
        put_2_dictionary(diccionario, &tam_dic, contenido[i]);
    }

    if (tam_dic == 0) {
        printf("No se pudo construir diccionario para %s\n", nombre_archivo);
        free(contenido);
        return;
    }

    Node** nodos = crear_nodos_desde_diccionario(diccionario, tam_dic);
    Node* raiz = construir_arbol_huffman(nodos, tam_dic);

    HuffmanCode tabla_codigos[MAX_SIZE];
    int total_codigos = 0;
    char codigo[MAX_CODE_LENGTH];
    generar_codigos(raiz, codigo, 0, tabla_codigos, &total_codigos);

    unsigned char len = strlen(nombre_archivo);
    if (len > 255) len = 255;
    fwrite(&len, 1, 1, salida);
    fwrite(nombre_archivo, 1, len, salida);

    unsigned int total_bits = 0;
    for (int i = 0; contenido[i] != '\0'; i++) {
        const char* cod = obtener_codigo(tabla_codigos, total_codigos, contenido[i]);
        if (cod) total_bits += strlen(cod);
    }
    fwrite(&total_bits, sizeof(unsigned int), 1, salida);

    fwrite(&total_codigos, 1, 1, salida);
    for (int i = 0; i < total_codigos; i++) {
        unsigned char largo = strlen(tabla_codigos[i].code);
        fwrite(&tabla_codigos[i].character, 1, 1, salida);
        fwrite(&largo, 1, 1, salida);

        unsigned char buffer = 0;
        int bits = 0;
        for (int j = 0; j < largo; j++) {
            buffer <<= 1;
            if (tabla_codigos[i].code[j] == '1') buffer |= 1;
            bits++;
            if (bits == 8) {
                fwrite(&buffer, 1, 1, salida);
                buffer = 0;
                bits = 0;
            }
        }
        if (bits > 0) {
            buffer <<= (8 - bits);
            fwrite(&buffer, 1, 1, salida);
        }
    }

    unsigned char buffer = 0;
    int llenos = 0;
    for (int i = 0; contenido[i] != '\0'; i++) {
        const char* cod = obtener_codigo(tabla_codigos, total_codigos, contenido[i]);
        if (!cod) continue;
        for (int j = 0; cod[j] != '\0'; j++) {
            buffer <<= 1;
            if (cod[j] == '1') buffer |= 1;
            llenos++;
            if (llenos == 8) {
                fwrite(&buffer, 1, 1, salida);
                buffer = 0;
                llenos = 0;
            }
        }
    }
    if (llenos > 0) {
        buffer <<= (8 - llenos);
        fwrite(&buffer, 1, 1, salida);
    }

    printf("Archivo comprimido: %s (%u bits)\n", nombre_archivo, total_bits);

    free(contenido);
    free(nodos);
    liberar_arbol(raiz);
}

int main() {
    DIR* dir = opendir("books");
    if (!dir) {
        printf("No se pudo abrir el directorio books/\n");
        return 1;
    }

    FILE* salida = fopen("comprimido.bin", "wb");
    if (!salida) {
        printf("No se pudo crear comprimido.bin\n");
        closedir(dir);
        return 1;
    }

    struct dirent* archivo;
    int cantidad = 0;

    while ((archivo = readdir(dir)) != NULL) {
        if (archivo->d_name && strstr(archivo->d_name, ".txt")) {
            cantidad++;
        }
    }

    rewinddir(dir);
    fwrite(&cantidad, 1, 1, salida);

    while ((archivo = readdir(dir)) != NULL) {
        if (archivo->d_name && strstr(archivo->d_name, ".txt")) {
            char ruta[512];
            snprintf(ruta, sizeof(ruta), "books/%s", archivo->d_name);
            comprimir_archivo(salida, ruta, archivo->d_name);
        }
    }

    fclose(salida);
    closedir(dir);

    printf("Compresión completada. Archivo generado: comprimido.bin\n");
    return 0;
}
