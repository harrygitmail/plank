#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btrfs.h"
#include "loader.h"
#include "common.h"
#include <sys/types.h>
#include <dirent.h>



char **read_conf_file(FILE *file)
{

	char *buffer = NULL;
	char **lines = NULL;

	enum plank_status ret = plank_OK;
	
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
				ret = plank_mem_err; 
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
	if(ret == plank_mem_err) {
		for(size_t i = 0; i < c_line; i++) free(lines[i]);

		free(lines);
		lines = NULL;
	}
		

	free(buffer);

	return lines;
}

struct loader_entries *list_loader_entries(char *BOOT, char *entry_token)
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

	files = list_files(path_to_files, &counts);

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
		int snap_found = 0;

		get_ker_ver_snap_tim(
			entries->entrie[i].file_name,
			tmp_kern_ver,
			&temp_snap_struct.snapshot_time);

		for(size_t j = 0; j < kern_list->counts; j++) {
			if((strcmp(kern_list->list[j].kernel_ver, tmp_kern_ver)) == 0) {
				found_kern = 1;
				entries->entrie[entrie_c].kern = &kern_list->list[j];
				break;
			}

		}

		for(size_t k = 0; k < snap_list->counts; k++) {
			if(
				temp_snap_struct.snapshot_time.tv_sec ==
				snap_list->list[k].snapshot_time.tv_sec
			  ) {
				snap_found = 1;
				entries->entrie[entrie_c].snap = &snap_list->list[k];
				break;
			}
		}

		if(found_kern == 1 && snap_found == 1) {
			entries->entrie[entrie_c++].status = ENTRY_OK;
		} else {
			entries->entrie[entrie_c++].status = ENTRY_DELETE_PENDING;
		}

	}

	entries->counts = entrie_c;

}
