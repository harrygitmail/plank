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

enum plank_status get_kernel_list(
	struct kernel_list *const ret,
	const char *root)
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

struct kernel_list *kernel_diff(
	const struct kernel_list *k1,
	const struct kernel_list *k2,
	enum kern_diff diff_type)
{
	size_t diff_c = 0;
	struct kernel_ver *list = NULL;

	const struct kernel_list *p1;
	const struct kernel_list *p2;

	int h;

	switch (diff_type) {
	case KERN_P1:
			p1 = k1;
			p2 = k2;
			h = 0;		// not common in list
			break;

	case KERN_P2:
			p1 = k2;
			p2 = k1;
			h = 0;		// not common in list
			break;

	case KERN_COM:
			p1 = k1;
			p2 = k2;
			h = 1;		// common in both list
			break;

	}

	list = malloc(sizeof(struct kernel_ver) * p1->counts);

	for (size_t i = 0; i < p1->counts; i++) {
		int found = 0;

		char *comp_1;
		comp_1 = p1->list[i].kernel_ver;

		for (size_t j = 0; j < p2->counts; j++) {

			char *comp_2;

			comp_2 = p2->list[j].kernel_ver;

			int c = strcmp(comp_1, comp_2);

			if (c == 0) {
				found = 1;
				break;
			}
		}

		if (found == h) {

			strcpy(list[diff_c].kernel_ver, p1->list[i].kernel_ver);
			++diff_c;

		}
	}

	if(diff_c == 0) {
		free(list);
		list = NULL;
	}

	struct kernel_list *ret = malloc(sizeof(struct kernel_list));
	ret->list = list;
	ret->counts = diff_c;

	return ret;
}
