#include <sys/ioctl.h>
#include <stdio.h>
#include "ioctl_ex34.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

/*
	argv doit contenir le path vers le fichier
	visé, puis le nombre de version
*/
int main(int argc, char **argv)
{
	int fd;
	struct parameters *p;
	int ret = 0;
	int size_filename;

	if (argc < 3) {
		printf("Paramètres manquants\n");
		goto end;
	}

	size_filename = strlen(argv[1]) + 1; /* inclure le \0 */
	p->size = size_filename;
	p = malloc(sizeof(struct parameters) + size_filename * (sizeof(char)));
	strcpy(p->cible, argv[1]);
	p->version = atoi(argv[2]);

	fd = open("/dev/current_view", O_RDWR);
	ret = ioctl(fd, SIZE, &size_filename);
	if (ret == -1) {
		printf("Error ioctl size path\n");
		goto desalloc;
	}
	ret = ioctl(fd, VERSION, p);
	if (ret == -1)
		printf("Error ioctl version\n");

desalloc:
	free(p);
end:
	printf("End of user program\n");
	return 0;
}