#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

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

void decodificar_archivo(FILE* f) {
    // Obtiene el largo del nombre del archivo
    unsigned char nombre_len;
    fread(&nombre_len, 1, 1, f);

    // Leer el nombre del archivo
    char nombre[256] = {0};
    fread(nombre, 1, nombre_len, f);
    nombre[nombre_len] = '\0';

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
        return;
    }

    // Lee el total de bits que se usaron para comprimir el archivo
    unsigned int total_bits;
    fread(&total_bits, sizeof(unsigned int), 1, f);

    // Lee todos los codigos de la tabla  y los almacena en "tabla"
    HuffmanCode tabla[MAX_CODIGOS];
    int total_codigos = leer_tabla(f, tabla);

    char codigo_actual[MAX_CODE_LENGTH] = {0};
    int pos = 0;
    unsigned int bits_leidos = 0;
    unsigned char byte;
    // fread() == 1, significa que se leyó un byte
    while (bits_leidos < total_bits && fread(&byte, 1, 1, f) == 1) {
        // Va leyendo bit por bit, hasta que encuentra un codigo completo (que este en la tabla)
        for (int i = 0; i < 8 && bits_leidos < total_bits; i++) {
            codigo_actual[pos] = leer_bit(byte, i) ? '1' : '0';
            pos++;
            codigo_actual[pos] = '\0';
            bits_leidos++;

            char c = buscar_codigo(tabla, total_codigos, codigo_actual);
            // Cuando encuentra un codigo completo, lo escribe en el archivo de salida
            if (c != '\0') {
                fputc(c, decompressed_file);
                pos = 0;
                codigo_actual[0] = '\0';
            }
        }
    }

    fclose(decompressed_file);
    printf("Archivo descomprimido: %s\n", nombre);
}

int main() {
    struct timeval start, end;
    gettimeofday(&start, NULL);
    FILE* f = fopen("comprimido.bin", "rb");
    if (!f) {
        printf("No se pudo abrir comprimido.bin\n");
        return 1;
    }

    unsigned char cantidad_archivos;
    fread(&cantidad_archivos, 1, 1, f);

    for (int i = 0; i < cantidad_archivos; i++) {
        decodificar_archivo(f);
    }

    fclose(f);

    gettimeofday(&end, NULL);
    double time_spent = (end.tv_sec - start.tv_sec) + 
                        (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Tiempo total de descompresión: %.2f segundos\n", time_spent);
    return 0;
}
