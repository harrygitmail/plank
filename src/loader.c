#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btrfs.h"
#include "loader.h"
#include "common.h"
#include "kernel.h"
#include "plank.h"
#include <sys/types.h>
#include <dirent.h>
#include <time.h>



char **read_file(FILE *file)
{

	char *buffer = NULL;
	char **lines = NULL;

	enum plank_status ret = PLANK_OK;
	
	size_t line_count = 6;
	size_t c_line = 0;

	ssize_t nread;
	size_t buffer_size;

	lines = malloc(sizeof(char *) * line_count);

	for(;;) {

		if(c_line - 1 <= line_count ) {
			char **tmp;
			size_t  new_line_count;
			new_line_count = line_count * 2;
			tmp = realloc(lines, new_line_count * sizeof(char *));
			if(tmp == NULL) {
				ret = PLANK_MEM_ERR;
				goto out;
			}

			lines = tmp;
			line_count = new_line_count;
		}


		nread = getline(&buffer, &buffer_size, file);
		if(nread == -1) {
			lines[c_line] = NULL;
			break;
		}

		buffer[nread - 1] = '\0';

		lines[c_line] = strdup(buffer);


		c_line++;

		lines[c_line] = NULL;
		
	}
		

out:
	if(ret == PLANK_MEM_ERR) {
		for(size_t i = 0; i < c_line; i++) free(lines[i]);

		free(lines);
		lines = NULL;
	}
		

	free(buffer);

	return lines;
}

struct loader_entries *list_loader_entries(const char *BOOT, const char *entry_token)
{
	char *path_to_files = NULL;
	struct loader_entrie *entries = NULL;
	char *prefix = NULL;
	char **files = NULL;

	int nread;
	int status;
	size_t c_line = 0;
	size_t prefix_lenght;
	size_t counts = 0;

	nread = asprintf(&path_to_files, "%s/loader/entries", BOOT);
	if(nread == -1) {
		entries = NULL;
		status = -1;
		goto out;
	}

	nread = asprintf(&prefix, "snapshot-%s", entry_token);

	if(nread == -1) {
		entries = NULL;
		status = -1;
		goto out;
	}

	prefix_lenght = strlen(prefix);

	files = list_files(path_to_files, 0,  &counts);

	entries = malloc(sizeof(struct loader_entrie) * counts);

	for (size_t i = 0; i < counts; i++) {

		int c = strncmp(files[i], prefix, prefix_lenght);

		if (c != 0) continue;

		entries[c_line++].file_name = strdup(files[i]);

	}

	if(c_line == 0) {
		free(entries);
		entries = NULL;
		status = 0;
		goto out;
	}

	status = 0;
out:
	free(path_to_files);
	free(prefix);
	free_file_list(files, counts);

	struct loader_entries *entrie_list;
	entrie_list = malloc(sizeof(struct loader_entries));
	entrie_list->entrie = entries;
	entrie_list->counts = c_line;
	entrie_list->status = status;

	return entrie_list;
}
static inline void update_entrie_status(int found_kern,
	int found_snap, entry_status *status) {

	switch (found_kern + found_snap) {
	case 2:
		*status = ENTRY_OK;
		break;
	default:
		*status = ENTRY_DELETE_PENDING;
	}

}

static inline int is_time_same(struct timespec t1, struct timespec t2)
{
	if(t1.tv_sec == t2.tv_sec) return 0;

	return 1;
}
void link_loader_entries(
	const kernel_list *kern_list,
	const struct snapshot_list *snap_list,
	struct loader_entries *const entries)
{
	size_t entrie_c = 0;

	snapshot_info temp_snap_struct;
	char tmp_kern_ver[65];

	for(size_t i = 0; i < entries->counts; i++) {
		int found_kern = 0;
		int found_snap = 0;

		get_ker_ver_snap_tim(
			entries->entrie[i].file_name,
			tmp_kern_ver,
			&temp_snap_struct.snapshot_time);

		for (size_t j = 0; j < kern_list->counts; j++) {

			char *comp_1 = kern_list->list[j].kernel_ver;
			char *comp_2 = tmp_kern_ver;

			if (strcmp(comp_1, comp_2) == 0) {

				found_kern = 1;

				struct kernel_ver *this;
				this = &kern_list->list[j];

				entries->entrie[entrie_c].kern = this;

				break;
			}

		}

		for (size_t k = 0; k < snap_list->counts; k++) {

			int c;
			c = is_time_same(
				snap_list->list[k].snapshot_time,
				temp_snap_struct.snapshot_time);

			if (c == 0 ) {

				found_snap = 1;

				snapshot_info *this;
				this = &snap_list->list[k];

				entries->entrie[entrie_c].snap = this;

				break;
			}
		}

		update_entrie_status(found_kern,
			found_snap, &entries->entrie[entrie_c++].status);
	}

	entries->counts = entrie_c;

}
