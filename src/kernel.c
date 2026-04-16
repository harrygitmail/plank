#include "btrfs.h"
#include "kernel.h"
#include <dirent.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

enum plank_status get_kernel_list(kernel_list **ret)
{
	DIR *kernel_modules = opendir("/lib/modules/");
	struct dirent *kernel;

	*ret = NULL;

	size_t n = 0;
	size_t capacity = 6;

	*ret = malloc(sizeof(kernel_list) * capacity);

	while((kernel = readdir(kernel_modules)) != NULL) {
		if(strncmp(kernel->d_name, ".", 1) == 0) continue;

		if(capacity <= n) {

			size_t new_capacity = capacity * 2;
			kernel_list *tmp;
			tmp = realloc(*ret, new_capacity);
			if (tmp == NULL) return plank_err;

			*ret = tmp;

		}

		strcpy((*ret)[n].kernel_ver, kernel->d_name);
		strcpy((*ret)[n + 1].kernel_ver, "STOP");
		n++;
	}

	closedir(kernel_modules);

	return plank_OK;
}
