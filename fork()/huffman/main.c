#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
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

Node* construir_arbol_huffman(Node** nodos, int total_archivos) {
    while (total_archivos > 1) {
        qsort(nodos, total_archivos, sizeof(Node*), comparar_nodos);

        Node* izquierda = nodos[0];
        Node* derecha = nodos[1];

        Node* nuevo = malloc(sizeof(Node));
        if (!nuevo) return NULL;

        nuevo->character = '\0';
        nuevo->frequency = izquierda->frequency + derecha->frequency;
        nuevo->left = izquierda;
        nuevo->right = derecha;

        nodos[0] = nuevo;
        nodos[1] = nodos[total_archivos - 1];
        total_archivos--;
    }

    return nodos[0];
}

void generar_codigos_aux(Node* raiz, char* codigo_actual, int profundidad, HuffmanCode* tabla, int* indice) {
    if (!raiz) return;

    if (!raiz->left && !raiz->right) {
        tabla[*indice].character = raiz->character;
        codigo_actual[profundidad] = '\0';
        strcpy(tabla[*indice].code, codigo_actual);
        (*indice)++;
        return;
    }

    codigo_actual[profundidad] = '0';
    generar_codigos_aux(raiz->left, codigo_actual, profundidad + 1, tabla, indice);

    codigo_actual[profundidad] = '1';
    generar_codigos_aux(raiz->right, codigo_actual, profundidad + 1, tabla, indice);
}

void generar_codigos(Node* raiz, HuffmanCode* tabla, int* indice) {
    char codigo_actual[MAX_CODE_LENGTH];
    generar_codigos_aux(raiz, codigo_actual, 0, tabla, indice);
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

char* comprimir_archivo(FILE* salida, const char* ruta_completa, const char* nombre_archivo, int indice_archivo) {
    // printf(ruta_completa);
    // printf("\n");
    FILE* f = fopen(ruta_completa, "r");
    if (!f) {
        printf("No se pudo abrir %s\n", ruta_completa);
        return NULL;
    }

    // Determinar el tamaño del archivo
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fseek(f, 0, SEEK_SET);

    // Verificar si el archivo está vacío
    if (tam <= 0) {
        printf("Archivo vacío: %s — se omite\n", nombre_archivo);
        fclose(f);
        return NULL;
    }

    // Reservar memoria para el contenido del archivo
    char* contenido = malloc(tam + 1);
    if (!contenido) {
        printf("Error asignando memoria para %s\n", nombre_archivo);
        fclose(f);
        return NULL;
    }

    // Leer todo el contenido del archivo
    fread(contenido, 1, tam, f);
    contenido[tam] = '\0';
    fclose(f);

    // Contar frecuencia de caracteres para el diccionario
    Dictionary diccionario[MAX_SIZE];
    int tam_dic = 0;
    for (int i = 0; contenido[i] != '\0'; i++) {
        put_2_dictionary(diccionario, &tam_dic, contenido[i]);
    }

    // Verificar que se pudo construir el diccionario
    if (tam_dic == 0) {
        printf("No se pudo construir diccionario para %s\n", nombre_archivo);
        free(contenido);
        return NULL;
    }

    // Crear nodos para el árbol de Huffman desde el diccionario
    Node** nodos = crear_nodos_desde_diccionario(diccionario, tam_dic);

    // Construir el árbol de Huffman
    Node* raiz = construir_arbol_huffman(nodos, tam_dic);

    // Generar tabla de códigos Huffman
    HuffmanCode tabla_codigos[MAX_SIZE];
    int total_codigos = 0;
    generar_codigos(raiz, tabla_codigos, &total_codigos);

    // Buffer para almacenar los datos comprimidos
    size_t buffer_size = (1024 * 1024)*10; // 10 MB
    char* buffer = malloc(buffer_size);
    if (!buffer) {
        printf("Error asignando memoria para el buffer\n");
        free(contenido);
        free(nodos);
        liberar_arbol(raiz);
        return NULL;
    }
    size_t buffer_pos = 0;

    // Escribir el largo del nombre del archivo
    unsigned char len = strlen(nombre_archivo);
    if (len > 255) len = 255;
    memcpy(buffer + buffer_pos, &len, 1);
    buffer_pos += 1;

    // Escribir el nombre del archivo
    memcpy(buffer + buffer_pos, nombre_archivo, len);
    buffer_pos += len;

    // Calcular el total de bits que tendrá el archivo comprimido
    unsigned int total_bits = 0;
    for (int i = 0; contenido[i] != '\0'; i++) {
        const char* cod = obtener_codigo(tabla_codigos, total_codigos, contenido[i]);
        if (cod) total_bits += strlen(cod);
    }

    // Escribir el total de bits
    memcpy(buffer + buffer_pos, &total_bits, sizeof(unsigned int));
    buffer_pos += sizeof(unsigned int);

    // Escribir el total de códigos en la tabla
    memcpy(buffer + buffer_pos, &total_codigos, 1);
    buffer_pos += 1;

    // Escribir cada entrada de la tabla de códigos
    for (int i = 0; i < total_codigos; i++) {
        // Escribir el caracter
        unsigned char largo = strlen(tabla_codigos[i].code);
        memcpy(buffer + buffer_pos, &tabla_codigos[i].character, 1);
        buffer_pos += 1;
        // Escribir el largo del código
        memcpy(buffer + buffer_pos, &largo, 1);
        buffer_pos += 1;

        // Convertir el código (string de 0s y 1s) a bits reales
        unsigned char temp_buffer = 0;
        int bits = 0;
        for (int j = 0; j < largo; j++) {
            temp_buffer <<= 1; // Desplazar los bits a la izquierda
            if (tabla_codigos[i].code[j] == '1') temp_buffer |= 1; // Agregar un 1 si corresponde
            bits++;
            // Si completamos un byte, lo escribimos al buffer
            if (bits == 8) {
                memcpy(buffer + buffer_pos, &temp_buffer, 1);
                buffer_pos += 1;
                temp_buffer = 0;
                bits = 0;
            }
        }
        // Si quedaron bits sin completar un byte, rellenamos con ceros
        if (bits > 0) {
            temp_buffer <<= (8 - bits);
            memcpy(buffer + buffer_pos, &temp_buffer, 1);
            buffer_pos += 1;
        }
    }

    // Escribir los datos comprimidos (contenido del archivo)
    unsigned char temp_buffer = 0;
    int llenos = 0;
    for (int i = 0; contenido[i] != '\0'; i++) {
        // Obtener el código Huffman para este caracter
        const char* cod = obtener_codigo(tabla_codigos, total_codigos, contenido[i]);
        if (!cod) continue; // Omitir caracteres sin código
        // Procesar cada bit del código
        for (int j = 0; cod[j] != '\0'; j++) {
            temp_buffer <<= 1; // Desplazar bits a la izquierda
            if (cod[j] == '1') temp_buffer |= 1; // Agregar un 1 si corresponde
            llenos++;
            // Si completamos un byte, lo escribimos al buffer
            if (llenos == 8) {
                memcpy(buffer + buffer_pos, &temp_buffer, 1);
                buffer_pos += 1;
                temp_buffer = 0;
                llenos = 0;
            }
        }
    }

    // Si quedaron bits sin completar un byte, rellenamos con ceros
    if (llenos > 0) {
        temp_buffer <<= (8 - llenos);
        memcpy(buffer + buffer_pos, &temp_buffer, 1);
        buffer_pos += 1;
    }

    free(contenido);
    free(nodos);
    liberar_arbol(raiz);

    char* temp_filename = malloc(40);
    sprintf(temp_filename, "books/huffman_temp_%d.bin", indice_archivo);
    
    // Crear el archivo temporal
    FILE* temp_file = fopen(temp_filename, "wb");
    if (!temp_file) {
        printf("Error al crear archivo temporal\n");
        free(buffer);
        free(contenido);
        free(nodos);
        liberar_arbol(raiz);
        exit(1);
    }
    // Escribir el buffer al archivo temporal
    fwrite(buffer, 1, buffer_pos, temp_file);
    fclose(temp_file);

    printf("Archivo comprimido: %s (%u bits)\n", nombre_archivo);

    return temp_filename;
}
int main() {
    printf("Iniciando compresión...\n");
    fflush(stdout);
    
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

    // Contar archivos .txt en el directorio
    int total_archivos = 0;
    char* rutas[256][256];
    char* nombres[256][256];
    while ((archivo = readdir(dir)) != NULL) {
        if (archivo->d_name && strstr(archivo->d_name, ".txt")) {
            // Copiar la ruta completa y su nombre en las respectivas listas
            char* nombre = strdup(archivo->d_name);
            char* ruta_completa = malloc(strlen("books/") + strlen(nombre) + 1);
            strcpy(ruta_completa, "books/");
            strcat(ruta_completa, nombre);
            rutas[total_archivos][0] = ruta_completa;
            nombres[total_archivos][0] = nombre;
            // Sumar el archivo al total
            total_archivos++;
        }
    }

    printf("Encontrados %d archivos .txt\n", total_archivos);
    fflush(stdout);
    
    // Se devuelve al inicio del directorio
    rewinddir(dir);

    // Escribir la cabecera del archivo comprimido (Total de archivos)
    fwrite(&total_archivos, 1, 1, salida);
    fclose(salida); // Cierra después de la cabecera

    int archivos_procesados = 0;
    while (archivos_procesados < total_archivos) {
        int id = fork();

        // Proceso hijo
        if (id == 0) {
            
            char nombre_archivo[20];
            snprintf(nombre_archivo, sizeof(nombre_archivo), "archivo_%d.txt", archivos_procesados);

            // Procesar el archivo
            char* archivo_comprimido = comprimir_archivo(salida, rutas[archivos_procesados][0], nombres[archivos_procesados][0], archivos_procesados);
            if (!archivo_comprimido) {
                printf("Error al comprimir %s\n", nombres[archivos_procesados][0]);
                exit(1);
            }

            exit(0);  // El hijo termina aquí
        } else {
            // Proceso padre
            archivos_procesados++;
        }
    }

    // El padre espera a todos los hijos
    for (int i = 0; i < total_archivos; i++) {
        wait(NULL);  // Espera a que todos los hijos terminen
    }

    // Escribir los archivos temporales en el archivo comprimido
    salida = fopen("comprimido.bin", "ab"); // Reabre en modo append
    if (!salida) {
        printf("No se pudo abrir comprimido.bin para añadir datos\n");
        // Manejo de error...
    }
    // Escribir los archivos temporales en el archivo comprimido y luego borrarlos
    for (int i = 0; i < total_archivos; i++) {
        char archivo_temporal[50];  
        sprintf(archivo_temporal, "books/huffman_temp_%d.bin", i);

        // Abrir el archivo temporal
        FILE* temp_file = fopen(archivo_temporal, "rb");
        if (!temp_file) {
            printf("Error al abrir archivo temporal %s\n", archivo_temporal);
            continue;
        }
        fseek(temp_file, 0, SEEK_END);
        long tam = ftell(temp_file);
        fseek(temp_file, 0, SEEK_SET);

        char* buffer = malloc(tam);
        if (!buffer) {
            printf("Error asignando memoria para el buffer\n");
            fclose(temp_file);
            continue;
        }

        // Leer el contenido del archivo temporal
        fread(buffer, 1, tam, temp_file);

        // Escribir el contenido en el archivo comprimido
        fwrite(buffer, 1, tam, salida);
        free(buffer);
        fclose(temp_file);

        // Borrar el archivo temporal después de procesarlo
        if (!remove(archivo_temporal) == 0) {
            printf("Error al eliminar archivo temporal %s\n", archivo_temporal);
        }
    }
    fflush(stdout);
    
    fclose(salida);
    closedir(dir);

    printf("Compresión completada. Archivo generado: comprimido.bin\n");
    return 0;
}