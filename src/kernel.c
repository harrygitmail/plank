#include <dirent.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "common.h"
#include "types.h"
#include "kernel.h"

enum plank_status get_kernel_list(kernel_list *const ret, const char *root)
{
	int root_fd = open(root, O_RDONLY);
	if(root_fd < 0 ) return PLANK_ERR;

	char *kernel_lib = "lib/modules";
	size_t counts = 0;
	char **list = list_files(kernel_lib, root_fd, &counts);

	ret->list = malloc(sizeof(struct kernel_ver) * counts);

	for(size_t i = 0; i < counts; i++) {

		strcpy(ret->list[i].kernel_ver, list[i]);

	}

	ret->counts = counts;
	free_file_list(list, counts);
	close(root_fd);

	return PLANK_OK;
}

kernel_list *kernel_diff(const kernel_list *k1, const kernel_list *k2)
{
	size_t diff_c = 0;
	struct kernel_ver *list = NULL;

	list = malloc(sizeof(struct kernel_ver) * k1->counts);

	for (size_t i = 0; i < k1->counts; i++) {
		int found = 0;

		char *comp_1;
		comp_1 = k1->list[i].kernel_ver;

		for (size_t j = 0; j < k2->counts; j++) {

			char *comp_2;

			comp_2 = k2->list[j].kernel_ver;

			int c = strcmp(comp_1, comp_2);

			if (c == 0) {
				found = 1;
				break;
			}
		}

		if (found == 0) {

			strcpy(list[diff_c].kernel_ver, k1->list[i].kernel_ver);
			++diff_c;

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
