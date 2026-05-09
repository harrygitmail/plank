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

kernel_list *kernel_diff(const kernel_list *k1, const kernel_list *k2)
{
	size_t diff_c = 0;
	struct kernel_ver *list = NULL;

	list = malloc(sizeof(struct kernel_ver) * k1->counts);

	for (size_t i = 0; i < k1->counts; i++) {
		int found = 0;

		for (size_t j = 0; j < k2->counts; j++) {

			char *comp_1;
			char *comp_2;

			comp_1 = k1->list[i].kernel_ver;
			comp_2 = k2->list[j].kernel_ver;

			int c = strcmp(comp_1, comp_2);

			if (c == 0) {
				found = 1;
				break;
			}
		}

		if (found == 0) {

			strcpy(list[diff_c++].kernel_ver, k1->list[i].kernel_ver);

		}
	}

	if(diff_c == 0) {
		free(list);
		list = NULL;
	}

	kernel_list *ret = malloc(sizeof(kernel_list));
	ret->list = list;
	ret->counts = diff_c;

	return ret;
}
