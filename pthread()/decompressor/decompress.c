
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_CODIGOS 256
#define MAX_CODE_LENGTH 256


typedef struct {
    char character;
    char code[MAX_CODE_LENGTH];
} HuffmanCode;


int leer_bit(unsigned char byte, int pos) {
    return (byte >> (7 - pos)) & 1;
}

int leer_tabla(FILE* f, HuffmanCode* tabla) {
    // Lee el total de códigos, del primer byte
    unsigned char total;
    fread(&total, 1, 1, f);


    for (int i = 0; i < total; i++) {
        // Lee el caracter y su largo
        unsigned char c, largo_codigo;
        fread(&c, 1, 1, f);
        fread(&largo_codigo, 1, 1, f);

        char buffer[MAX_CODE_LENGTH] = {0};
        int bits_leidos = 0;
        // Va leyendo de byte en byte para obtener el codigo del caracter(c)
        // Despues de la primera iteracion, si faltaran bits, se lee el siguiente byte y se obtienen los faltantes
        // Para eso el while
        while (bits_leidos < largo_codigo) {
            unsigned char byte;
            fread(&byte, 1, 1, f);
            // Lee cada bit del byte y lo almacena en el buffer
            for (int j = 0; j < 8 && bits_leidos < largo_codigo; j++) {
                buffer[bits_leidos++] = leer_bit(byte, j) ? '1' : '0';
            }
        }
        
        // Agrega el caracter, y su codigo a la tabla
        tabla[i].character = c;
        strcpy(tabla[i].code, buffer);
    }

    return total;
}

// Busca el caracter en la tabla de codigos
// Si lo encuentra, devuelve el caracter, si no, devuelve '\0'
char buscar_codigo(HuffmanCode* tabla, int total, const char* codigo) {
    for (int i = 0; i < total; i++) {
        if (strcmp(tabla[i].code, codigo) == 0)
            return tabla[i].character;
    }
    return '\0';
}
// Función para determinar las posiciones de inicio de cada archivo
long* obtener_posiciones_archivos(FILE* f, int cantidad_archivos) {
    long* posiciones = malloc((cantidad_archivos + 1) * sizeof(long));
    if (!posiciones) return NULL;
    
    // La primera posición es justo después del byte de cantidad
    posiciones[0] = 1;
    
    for (int i = 0; i < cantidad_archivos; i++) {
        fseek(f, posiciones[i], SEEK_SET);
        
        // Leer longitud del nombre
        unsigned char nombre_len;
        fread(&nombre_len, 1, 1, f);
        
        // Saltamos el nombre
        fseek(f, nombre_len, SEEK_CUR);
        
        // Leer total_bits
        unsigned int total_bits;
        fread(&total_bits, sizeof(unsigned int), 1, f);
        
        // Leer total_codigos
        unsigned char total_codigos;
        fread(&total_codigos, 1, 1, f);
        
        // Para cada código en la tabla
        for (int j = 0; j < total_codigos; j++) {
            unsigned char c, largo_codigo;
            fread(&c, 1, 1, f);
            fread(&largo_codigo, 1, 1, f);
            
            // Calcular bytes que ocupa el código y saltarlos
            int bytes_codigos = (largo_codigo + 7) / 8;
            fseek(f, bytes_codigos, SEEK_CUR);
        }
        
        // Calcular bytes que ocupan los datos comprimidos
        int bytes_datos = (total_bits + 7) / 8;
        
        // Saltamos los datos comprimidos
        fseek(f, bytes_datos, SEEK_CUR);
        
        // La posición actual es el inicio del siguiente archivo
        posiciones[i+1] = ftell(f);
    }
    
    return posiciones;
}
pthread_mutex_t file_mutex;

typedef struct {
    FILE* archivo_entrada;
    long posicion_inicio;
    int indice_archivo;
} ThreadData;

// Función para descomprimir un archivo (versión para hilos)
void* decodificar_archivo_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    FILE* f = data->archivo_entrada;
    
    // Bloquear el archivo para uso exclusivo
    pthread_mutex_lock(&file_mutex);
    
    // Posicionarse en el inicio de este archivo
    fseek(f, data->posicion_inicio, SEEK_SET);
    
    // Obtener el largo del nombre
    unsigned char nombre_len;
    fread(&nombre_len, 1, 1, f);
    
    // Leer el nombre del archivo
    char nombre[256] = {0};
    fread(nombre, 1, nombre_len, f);
    nombre[nombre_len] = '\0';
    
    // Leer el total de bits
    unsigned int total_bits;
    fread(&total_bits, sizeof(unsigned int), 1, f);
    
    // Leer la tabla de códigos
    HuffmanCode tabla[MAX_CODIGOS];
    int total_codigos = leer_tabla(f, tabla);
    
    // Crear un buffer para leer todos los datos de una vez
    long posicion_actual = ftell(f);
    int bytes_datos = (total_bits + 7) / 8;
    unsigned char* buffer_datos = malloc(bytes_datos);
    
    if (!buffer_datos) {
        pthread_mutex_unlock(&file_mutex);
        printf("Error: No se pudo asignar memoria para el archivo %s\n", nombre);
        return NULL;
    }
    
    // Leer todos los datos comprimidos de una vez
    fread(buffer_datos, 1, bytes_datos, f);
    
    // Ya podemos liberar el archivo para otros hilos
    pthread_mutex_unlock(&file_mutex);
    
    // Crear el directorio "books" si no existe
    struct stat st = {0};
    if (stat("books", &st) == -1) {
        mkdir("books", 0755);
    }
    // Crear el archivo de salida en el directorio "books"
    char ruta_salida[512];
    snprintf(ruta_salida, sizeof(ruta_salida), "books/%s", nombre);
    FILE* decompressed_file = fopen(ruta_salida, "w");
    if (!decompressed_file) {
        printf("No se pudo crear %s\n", ruta_salida);
        free(buffer_datos);
        return NULL;
    }
    
    // Procesar los datos del buffer sin acceder más al archivo comprimido
    char codigo_actual[MAX_CODE_LENGTH] = {0};
    int pos = 0;
    unsigned int bits_leidos = 0;
    
    for (int byte_idx = 0; byte_idx < bytes_datos && bits_leidos < total_bits; byte_idx++) {
        unsigned char byte = buffer_datos[byte_idx];
        
        for (int i = 0; i < 8 && bits_leidos < total_bits; i++) {
            codigo_actual[pos] = leer_bit(byte, i) ? '1' : '0';
            pos++;
            codigo_actual[pos] = '\0';
            bits_leidos++;

            char c = buscar_codigo(tabla, total_codigos, codigo_actual);
            if (c != '\0') {
                fputc(c, decompressed_file);
                pos = 0;
                codigo_actual[0] = '\0';
            }
        }
    }
    
    fclose(decompressed_file);
    free(buffer_datos);
    printf("Archivo #%d descomprimido: %s (%u bits)\n", 
           data->indice_archivo, nombre, total_bits);
    
    return NULL;
}



int main() {
    FILE* f = fopen("comprimido.bin", "rb");
    if (!f) {
        printf("No se pudo abrir comprimido.bin\n");
        return 1;
    }

    unsigned char cantidad_archivos;
    fread(&cantidad_archivos, 1, 1, f);
    printf("Descomprimiendo %d archivos en paralelo...\n", cantidad_archivos);
    
    // Obtener las posiciones de inicio de cada archivo
    long* posiciones = obtener_posiciones_archivos(f, cantidad_archivos);
    if (!posiciones) {
        printf("Error determinando posiciones de los archivos\n");
        fclose(f);
        return 1;
    }
    
    // Crear los hilos y sus datos
    pthread_t* threads = malloc(cantidad_archivos * sizeof(pthread_t));
    ThreadData* thread_data = malloc(cantidad_archivos * sizeof(ThreadData));
    
    if (!threads || !thread_data) {
        printf("Error asignando memoria para hilos\n");
        free(posiciones);
        if (threads) free(threads);
        if (thread_data) free(thread_data);
        fclose(f);
        return 1;
    }
    
    // Iniciar los hilos
    for (int i = 0; i < cantidad_archivos; i++) {
        thread_data[i].archivo_entrada = f;
        thread_data[i].posicion_inicio = posiciones[i];
        thread_data[i].indice_archivo = i;
        
        if (pthread_create(&threads[i], NULL, decodificar_archivo_thread, &thread_data[i]) != 0) {
            printf("Error creando hilo %d\n", i);
        }
    }
    
    // Esperar a que todos los hilos terminen
    for (int i = 0; i < cantidad_archivos; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Liberar memoria y cerrar archivo
    free(posiciones);
    free(threads);
    free(thread_data);
    fclose(f);
    pthread_mutex_destroy(&file_mutex);
    
    printf("Descompresión completada con éxito.\n");
    return 0;
}