#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "node.h"
#include "dictionary.h"

#define MAX_SIZE 1920
#define MAX_CODE_LENGTH 256
#define SEPARADOR 0x01

typedef struct {
    char character;
    char code[MAX_CODE_LENGTH];
} HuffmanCode;

// Crear nodos desde el diccionario
Node** crear_nodos_desde_diccionario(Dictionary diccionario[], int tamaño) {
    Node** nodos = malloc(tamaño * sizeof(Node*));
    for (int i = 0; i < tamaño; i++) {
        Node* nuevo = malloc(sizeof(Node));
        nuevo->character = diccionario[i].key;
        nuevo->frequency = diccionario[i].value;
        nuevo->left = NULL;
        nuevo->right = NULL;
        nodos[i] = nuevo;
    }
    return nodos;
}

// Leer todos los archivos .txt del directorio
char* leer_archivos_txt(const char* carpeta) {
    DIR* dir = opendir(carpeta);
    if (dir == NULL) {
        printf("No se pudo abrir el directorio.\n");
        return NULL;
    }

    struct dirent* archivo;
    char* contenido_total = malloc(1);
    contenido_total[0] = '\0';
    int longitud_total = 0;

    while ((archivo = readdir(dir)) != NULL) {
        if (strstr(archivo->d_name, ".txt") == NULL) continue;

        char ruta[512];
        snprintf(ruta, sizeof(ruta), "%s/%s", carpeta, archivo->d_name);

        FILE* f = fopen(ruta, "r");
        if (f == NULL) continue;

        fseek(f, 0, SEEK_END);
        int longitud = ftell(f);
        fseek(f, 0, SEEK_SET);

        char* buffer = malloc(longitud + 2);
        fread(buffer, 1, longitud, f);
        fclose(f);

        buffer[longitud] = SEPARADOR;
        buffer[longitud + 1] = '\0';

        contenido_total = realloc(contenido_total, longitud_total + longitud + 2);
        strcat(contenido_total, buffer);
        longitud_total += longitud + 1;

        free(buffer);
    }

    closedir(dir);
    return contenido_total;
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

void imprimir_arbol(Node* raiz, int nivel) {
    if (raiz == NULL) return;
    for (int i = 0; i < nivel; i++) printf("  ");
    if (raiz->character != '\0')
        printf("'%c': %d\n", raiz->character, raiz->frequency);
    else
        printf("[Interno]: %d\n", raiz->frequency);
    imprimir_arbol(raiz->left, nivel + 1);
    imprimir_arbol(raiz->right, nivel + 1);
}

void liberar_arbol(Node* nodo) {
    if (!nodo) return;
    liberar_arbol(nodo->left);
    liberar_arbol(nodo->right);
    free(nodo);
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

// Buscar código de un carácter
const char* obtener_codigo(HuffmanCode* tabla, int total, char caracter) {
    for (int i = 0; i < total; i++) {
        if (tabla[i].character == caracter) {
            return tabla[i].code;
        }
    }
    return NULL;
}

// Escribir archivo binario codificado
void escribir_bits_en_archivo(const char* nombre_archivo, char* texto, HuffmanCode* tabla, int total) {
    FILE* f = fopen(nombre_archivo, "wb");
    if (!f) {
        printf("No se pudo abrir el archivo binario.\n");
        return;
    }

    unsigned char buffer = 0;
    int bits_llenos = 0;

    for (int i = 0; texto[i] != '\0'; i++) {
        const char* codigo = obtener_codigo(tabla, total, texto[i]);
        if (!codigo) continue;

        for (int j = 0; codigo[j] != '\0'; j++) {
            buffer <<= 1;
            if (codigo[j] == '1') buffer |= 1;
            bits_llenos++;

            if (bits_llenos == 8) {
                fwrite(&buffer, 1, 1, f);
                bits_llenos = 0;
                buffer = 0;
            }
        }
    }

    if (bits_llenos > 0) {
        buffer <<= (8 - bits_llenos);
        fwrite(&buffer, 1, 1, f);
    }

    fclose(f);
    printf("Archivo binario comprimido creado: %s\n", nombre_archivo);
}

// MAIN
int main() {
    char* contenido = leer_archivos_txt("books");
    if (contenido == NULL) return 1;

    Dictionary diccionario[MAX_SIZE];
    int tam = 0;

    for (int i = 0; contenido[i] != '\0'; i++) {
        put_2_dictionary(diccionario, &tam, contenido[i]);
    }

    Node** nodos = crear_nodos_desde_diccionario(diccionario, tam);
    Node* raiz = construir_arbol_huffman(nodos, tam);

    HuffmanCode tabla_codigos[MAX_SIZE];
    int total_codigos = 0;
    char codigo[MAX_CODE_LENGTH];

    generar_codigos(raiz, codigo, 0, tabla_codigos, &total_codigos);

    escribir_bits_en_archivo("comprimido.bin", contenido, tabla_codigos, total_codigos);

    free(nodos);
    free(contenido);
    liberar_arbol(raiz);

    return 0;
}
