#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define CANT_ARGUMENTOS_VALIDOS 3
#define ERROR_ARGUMENTOS -1
#define ERROR_APERTURA_ARCHIVO -1
#define ERROR_ARCHIVO -1
#define HAY_ERROR -1
#define ERROR_ES_DIRECTORIO -1
#define PERMISOS_ARCHIVO 0666

void imprimir_error(char* nombre_archivo, int errnum){

	fprintf(stderr, "Error al abrir %s: %s\n", nombre_archivo, strerror(errnum));
}

bool hay_error(int errnum, char* nombre_archivo){

	if(errnum == EISDIR){
		fprintf(stderr, "Error: el archivo %s es un directorio.\n", nombre_archivo);
		return true;
	}

	if(errnum == EINTR) return false;

	if(errnum != 0){
		perror("Error");
		return true;
	}

	return false;
}

int copiar_datos(int archivo_fuente, int archivo_destino, char* nombre_archivo_fuente, char* nombre_archivo_destino){

	uint8_t buf[512];
	ssize_t bytes_leidos;

	while((bytes_leidos = read(archivo_fuente, buf, sizeof(buf)))){
		if(hay_error(errno, nombre_archivo_fuente)) return HAY_ERROR;

		ssize_t bytes_escritos = 0;
		ssize_t bytes_a_escribir = bytes_leidos-bytes_escritos;

		do{
			bytes_escritos += write(archivo_destino, (buf+bytes_escritos), bytes_a_escribir);
			bytes_a_escribir = bytes_leidos-bytes_escritos;

			if(hay_error(errno, nombre_archivo_destino)) return HAY_ERROR;

		}while(bytes_escritos < bytes_leidos);
	}

	return 0;
}

int procesar_archivos(char* nombre_archivo_fuente, char* nombre_archivo_destino){

	int archivo_fuente = open(nombre_archivo_fuente, O_RDONLY);

	if(archivo_fuente == ERROR_APERTURA_ARCHIVO){
		imprimir_error(nombre_archivo_fuente, errno);
		return ERROR_ARCHIVO;
	}

	int archivo_destino = open(nombre_archivo_destino, O_CREAT | O_WRONLY | O_TRUNC, PERMISOS_ARCHIVO);

	if(archivo_destino == ERROR_APERTURA_ARCHIVO){
		imprimir_error(nombre_archivo_destino, errno);
		close(archivo_fuente);
		return ERROR_ARCHIVO;
	}

	int valor_retorno = copiar_datos(archivo_fuente, archivo_destino, nombre_archivo_fuente, nombre_archivo_destino);

	close(archivo_fuente);
	close(archivo_destino);

	return valor_retorno;
}

int main(int argc, char* argv[]){

	if(argc != CANT_ARGUMENTOS_VALIDOS){
		fprintf(stderr, "Cantidad inválida de argumentos.\n");
		return ERROR_ARGUMENTOS;	
	}

	return procesar_archivos(argv[1], argv[2]);
}