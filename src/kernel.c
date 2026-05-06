#include "btrfs.h"
#include "kernel.h"
#include <dirent.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

enum plank_status get_kernel_list(kernel_list *ret)
{
	char *kernel_lib = "/lib/modules";
	size_t counts = 0;
	char **list = list_files(kernel_lib, &counts);

	ret->list = malloc(sizeof(struct kernel_ver) * counts);
	for(size_t i = 0; i < counts; i++) {

		strcpy(ret->list[i].kernel_ver, list[i]);

	}

	ret->counts = counts;
	free_file_list(list, counts);

	return plank_OK;
}
