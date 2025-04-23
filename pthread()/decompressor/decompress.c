#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

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
    unsigned char total;
    fread(&total, 1, 1, f);

    for (int i = 0; i < total; i++) {
        unsigned char c, largo;
        fread(&c, 1, 1, f);
        fread(&largo, 1, 1, f);

        char buffer[MAX_CODE_LENGTH] = {0};
        int bits_leidos = 0;
        while (bits_leidos < largo) {
            unsigned char byte;
            fread(&byte, 1, 1, f);
            for (int j = 0; j < 8 && bits_leidos < largo; j++) {
                buffer[bits_leidos++] = leer_bit(byte, j) ? '1' : '0';
            }
        }

        tabla[i].character = c;
        strcpy(tabla[i].code, buffer);
    }

    return total;
}

char buscar_codigo(HuffmanCode* tabla, int total, const char* codigo) {
    for (int i = 0; i < total; i++) {
        if (strcmp(tabla[i].code, codigo) == 0)
            return tabla[i].character;
    }
    return '\0';
}

void decodificar_archivo(FILE* f, const char* nombre) {
    unsigned char nombre_len;
    fread(&nombre_len, 1, 1, f);

    char nombre_archivo[256] = {0};
    fread(nombre_archivo, 1, nombre_len, f);
    nombre_archivo[nombre_len] = '\0';

    unsigned int total_bits;
    fread(&total_bits, sizeof(unsigned int), 1, f);

    HuffmanCode tabla[MAX_CODIGOS];
    int total_codigos = leer_tabla(f, tabla);

    char codigo_actual[MAX_CODE_LENGTH] = {0};
    int pos = 0;
    unsigned int bits_leidos = 0;
    unsigned char byte;

    FILE* out = fopen(nombre_archivo, "w");
    if (!out) {
        printf("No se pudo crear %s\n", nombre_archivo);
        return;
    }

    while (bits_leidos < total_bits && fread(&byte, 1, 1, f) == 1) {
        for (int i = 0; i < 8 && bits_leidos < total_bits; i++) {
            codigo_actual[pos++] = leer_bit(byte, i) ? '1' : '0';
            codigo_actual[pos] = '\0';
            bits_leidos++;

            char c = buscar_codigo(tabla, total_codigos, codigo_actual);
            if (c != '\0') {
                fputc(c, out);
                pos = 0;
                codigo_actual[0] = '\0';
            }
        }
    }

    fclose(out);
    printf("Archivo descomprimido: %s\n", nombre_archivo);
}

int main() {
    FILE* f = fopen("comprimido.bin", "rb");
    if (!f) {
        printf("No se pudo abrir comprimido.bin\n");
        return 1;
    }

    unsigned char cantidad_archivos;
    fread(&cantidad_archivos, 1, 1, f);

    pid_t pid;
    for (int i = 0; i < cantidad_archivos; i++) {
        pid = fork(); // Crea un nuevo proceso para cada archivo
        if (pid == 0) { // Si es el hijo
            decodificar_archivo(f, "salida.txt"); // Aquí la ruta debe ser dinámica
            fclose(f);
            exit(0); // El hijo termina después de decodificar
        }
    }

    // Padre espera a todos los hijos
    for (int i = 0; i < cantidad_archivos; i++) {
        wait(NULL);
    }

    fclose(f);
    return 0;
}
