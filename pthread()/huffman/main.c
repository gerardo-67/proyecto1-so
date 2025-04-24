#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <pthread.h>
#include "node.h"
#include "dictionary.h"

#define MAX_SIZE 1920
#define MAX_CODE_LENGTH 256

pthread_mutex_t mutex;

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

typedef struct ThreadData {
    FILE* salida;
    const char* ruta_completa;
    const char* nombre_archivo;
} ThreadData;
void* comprimir_archivo(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    if (!data->salida || !data->ruta_completa || !data->nombre_archivo) {
        printf("Datos del hilo inválidos\n");
        return NULL;
    }
    const char* ruta_completa = data->ruta_completa;
    const char* nombre_archivo = data->nombre_archivo;

    FILE* f = fopen(ruta_completa, "r");
    if (!f) {
        printf("No se pudo abrir %s\n", ruta_completa);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long tam = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (tam <= 0) {
        printf("Archivo vacío: %s — se omite\n", nombre_archivo);
        fclose(f);
        return NULL;
    }

    char* contenido = malloc(tam + 1);
    if (!contenido) {
        printf("Error asignando memoria para %s\n", nombre_archivo);
        fclose(f);
        return NULL;
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
        return NULL;
    }

    Node** nodos = crear_nodos_desde_diccionario(diccionario, tam_dic);
    Node* raiz = construir_arbol_huffman(nodos, tam_dic);

    HuffmanCode tabla_codigos[MAX_SIZE];
    int total_codigos = 0;
    generar_codigos(raiz, tabla_codigos, &total_codigos);

    // Buffer para almacenar los datos
    size_t buffer_size = (1024 * 1024)*10; // 1 MB
    char* buffer = malloc(buffer_size);
    if (!buffer) {
        printf("Error asignando memoria para el buffer\n");
        free(contenido);
        free(nodos);
        liberar_arbol(raiz);
        return NULL;
    }
    size_t buffer_pos = 0;

    unsigned char len = strlen(nombre_archivo);
    if (len > 255) len = 255;
    memcpy(buffer + buffer_pos, &len, 1);
    buffer_pos += 1;
    memcpy(buffer + buffer_pos, nombre_archivo, len);
    buffer_pos += len;

    unsigned int total_bits = 0;
    for (int i = 0; contenido[i] != '\0'; i++) {
        const char* cod = obtener_codigo(tabla_codigos, total_codigos, contenido[i]);
        if (cod) total_bits += strlen(cod);
    }
    memcpy(buffer + buffer_pos, &total_bits, sizeof(unsigned int));
    buffer_pos += sizeof(unsigned int);

    memcpy(buffer + buffer_pos, &total_codigos, 1);
    buffer_pos += 1;
    for (int i = 0; i < total_codigos; i++) {
        unsigned char largo = strlen(tabla_codigos[i].code);
        memcpy(buffer + buffer_pos, &tabla_codigos[i].character, 1);
        buffer_pos += 1;
        memcpy(buffer + buffer_pos, &largo, 1);
        buffer_pos += 1;

        unsigned char temp_buffer = 0;
        int bits = 0;
        for (int j = 0; j < largo; j++) {
            temp_buffer <<= 1;
            if (tabla_codigos[i].code[j] == '1') temp_buffer |= 1;
            bits++;
            if (bits == 8) {
                memcpy(buffer + buffer_pos, &temp_buffer, 1);
                buffer_pos += 1;
                temp_buffer = 0;
                bits = 0;
            }
        }
        if (bits > 0) {
            temp_buffer <<= (8 - bits);
            memcpy(buffer + buffer_pos, &temp_buffer, 1);
            buffer_pos += 1;
        }
    }

    unsigned char temp_buffer = 0;
    int llenos = 0;
    for (int i = 0; contenido[i] != '\0'; i++) {
        const char* cod = obtener_codigo(tabla_codigos, total_codigos, contenido[i]);
        if (!cod) continue;
        for (int j = 0; cod[j] != '\0'; j++) {
            temp_buffer <<= 1;
            if (cod[j] == '1') temp_buffer |= 1;
            llenos++;
            if (llenos == 8) {
                memcpy(buffer + buffer_pos, &temp_buffer, 1);
                buffer_pos += 1;
                temp_buffer = 0;
                llenos = 0;
            }
        }
    }
    if (llenos > 0) {
        temp_buffer <<= (8 - llenos);
        memcpy(buffer + buffer_pos, &temp_buffer, 1);
        buffer_pos += 1;
    }

    // Escribir el buffer en el archivo de salida

    pthread_mutex_lock(&mutex);
    fwrite(buffer, 1, buffer_pos, data->salida);
    pthread_mutex_unlock(&mutex);

    printf("Archivo comprimido: %s (%u bits)\n", nombre_archivo, total_bits);

    free(buffer);
    free(contenido);
    free(nodos);
    liberar_arbol(raiz);
}
int main() {
    printf("Iniciando compresión...\n");
    fflush(stdout);
    
    pthread_mutex_init(&mutex, NULL);
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

    // Contar archivos .txt
    while ((archivo = readdir(dir)) != NULL) {
        if (archivo->d_name && strstr(archivo->d_name, ".txt")) {
            cantidad++;
        }
    }

    printf("Encontrados %d archivos .txt\n", cantidad);
    fflush(stdout);

    // Reservar memoria para los hilos y los datos de los hilos
    pthread_t* threads = malloc(cantidad * sizeof(pthread_t));
    ThreadData** thread_data_array = malloc(cantidad * sizeof(ThreadData*));
    
    if (!threads || !thread_data_array) {
        printf("Error asignando memoria\n");
        fclose(salida);
        closedir(dir);
        if (threads) free(threads);
        if (thread_data_array) free(thread_data_array);
        return 1;
    }
    
    // Se devuelve al inicio del directorio
    rewinddir(dir);
    fwrite(&cantidad, 1, 1, salida);
    
    int hilo_actual = 0;
    while ((archivo = readdir(dir)) != NULL) {
        // Se omiten los no .txt
        if (archivo->d_name && strstr(archivo->d_name, ".txt")) {
            // Se crea espacio para los datos del hilo
            ThreadData* data = malloc(sizeof(ThreadData));
            if (!data) {
                printf("Error asignando memoria para los datos del hilo\n");
                fclose(salida);
                closedir(dir);
                free(threads);
                
                // Liberar datos de hilos ya creados
                for (int i = 0; i < hilo_actual; i++) {
                    if (thread_data_array[i]) {
                        if (thread_data_array[i]->ruta_completa)
                            free((void*)thread_data_array[i]->ruta_completa);
                        if (thread_data_array[i]->nombre_archivo)
                            free((void*)thread_data_array[i]->nombre_archivo);
                        free(thread_data_array[i]);
                    }
                }
                free(thread_data_array);
                return 1;
            }
            
            char* nombre = strdup(archivo->d_name);
            char* ruta_completa = malloc(strlen("books/") + strlen(nombre) + 1);
            
            // Si no se pudo asignar memoria para la ruta completa o el nombre
            if (!nombre || !ruta_completa) {
                printf("Error asignando memoria para rutas\n");
                if (nombre) free(nombre);
                if (ruta_completa) free(ruta_completa);
                free(data);
                
                // Liberar datos anteriores
                for (int i = 0; i < hilo_actual; i++) {
                    if (thread_data_array[i]) {
                        if (thread_data_array[i]->ruta_completa)
                            free((void*)thread_data_array[i]->ruta_completa);
                        if (thread_data_array[i]->nombre_archivo)
                            free((void*)thread_data_array[i]->nombre_archivo);
                        free(thread_data_array[i]);
                    }
                }
                
                free(thread_data_array);
                free(threads);
                fclose(salida);
                closedir(dir);
                return 1;
            }
            
            strcpy(ruta_completa, "books/");
            strcat(ruta_completa, nombre);

            fflush(stdout);
            
            data->salida = salida;
            data->ruta_completa = ruta_completa;
            data->nombre_archivo = nombre;
            
            thread_data_array[hilo_actual] = data;
            // Crear hilo para comprimir el archivo
            if (pthread_create(&threads[hilo_actual], NULL, comprimir_archivo, (void*)data) != 0) {
                printf("Error creando hilo para %s\n", nombre);
                
                // Liberar memoria actual y anterior
                free((void*)thread_data_array[hilo_actual]->ruta_completa);
                free((void*)thread_data_array[hilo_actual]->nombre_archivo);
                free(thread_data_array[hilo_actual]);
                
                for (int i = 0; i < hilo_actual; i++) {
                    if (thread_data_array[i]) {
                        if (thread_data_array[i]->ruta_completa)
                            free((void*)thread_data_array[i]->ruta_completa);
                        if (thread_data_array[i]->nombre_archivo)
                            free((void*)thread_data_array[i]->nombre_archivo);
                        free(thread_data_array[i]);
                    }
                }
                
                free(thread_data_array);
                free(threads);
                fclose(salida);
                closedir(dir);
                return 1;
            }
            // Incrementar el número de hilos activos
            hilo_actual++;
        }
    }

    printf("Esperando a que terminen %d hilos...\n", hilo_actual);
    fflush(stdout);
    
    // Unir todos los hilos
    for (int i = 0; i < hilo_actual; i++) {
        pthread_join(threads[i], NULL);
        
        // Limpiar la memoria después de que cada hilo termina
        if (thread_data_array[i]) {
            if (thread_data_array[i]->ruta_completa)
                free((void*)thread_data_array[i]->ruta_completa);
            if (thread_data_array[i]->nombre_archivo)
                free((void*)thread_data_array[i]->nombre_archivo);
            free(thread_data_array[i]);
        }
    }

    free(thread_data_array);
    free(threads);
    fclose(salida);
    closedir(dir);

    pthread_mutex_destroy(&mutex);
    printf("Compresión completada. Archivo generado: comprimido.bin\n");
    return 0;
}