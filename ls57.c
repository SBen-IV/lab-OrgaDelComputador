#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define DIRECTORIO_ACTUAL "./"
#define ERROR_ABRIR_DIRECTORIO -1
#define ARCHIVO_OCULTO '.'

void imprimir_datos(char* nombre_archivo, int tipo_archivo, size_t tamanio){
	if(S_ISREG(tipo_archivo)) fprintf(stdout, "%s\t%lu\n", nombre_archivo, tamanio);

	else fprintf(stdout, "%s\n", nombre_archivo);
}

int mostrar_datos(DIR* directorio_actual, int directorio_num){

	struct dirent* archivo_leido;

	while((archivo_leido = readdir(directorio_actual))){
		struct stat datos_archivo;

		fstatat(directorio_num, archivo_leido->d_name, &datos_archivo, 0);

		if(archivo_leido->d_name[0] != ARCHIVO_OCULTO){		
			imprimir_datos(archivo_leido->d_name, datos_archivo.st_mode, datos_archivo.st_size);
		}
	}

	return 0;
}

int procesar_directorio(char* nombre_directorio){

	int directorio_num = open(nombre_directorio, O_RDONLY | O_DIRECTORY);

	if(directorio_num == ERROR_ABRIR_DIRECTORIO){
		perror("Error");
		return ERROR_ABRIR_DIRECTORIO;
	}

	DIR* directorio_actual = fdopendir(directorio_num);

	if(errno == ENOTDIR){
		fprintf(stderr, "Sólo se pueden listar directorios.\n");
		return ERROR_ABRIR_DIRECTORIO;
	}else if(!directorio_actual){
		perror("Error");
		return ERROR_ABRIR_DIRECTORIO;
	}

	mostrar_datos(directorio_actual, directorio_num);

	closedir(directorio_actual);

	return 0;
}

int main(int argc, char* argv[]){

	if(argc == 2) procesar_directorio(argv[1]);
	else procesar_directorio(DIRECTORIO_ACTUAL);

	return 0;
}