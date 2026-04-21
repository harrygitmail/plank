#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btrfs.h"
#include "loader.h"
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
#define MAXLEN 255

	char *path_to_files = NULL;
	char **list = NULL;
	char *prefix = NULL;

	DIR *dir = NULL;
	struct dirent *entries;

	int nread;
	int status;
	size_t capacity = 5;
	size_t c_line = 0;
	size_t prefix_lenght;

	nread = asprintf(&path_to_files, "%s/loader/entries", BOOT);
	if(nread == -1) {
		list = NULL;
		status = -1;
		goto out;
	}

	nread = asprintf(&prefix, "snapshot-%s", entry_token);

	if(nread == -1) {
		list = NULL;
		status = -1;
		goto out;
	}

	prefix_lenght = strlen(prefix);

	dir = opendir(path_to_files);

	list = malloc(sizeof(char *) * capacity);

	if(list == NULL) {
		status = -1;
		goto out;
	}

	while((entries = readdir(dir)) != NULL) {

		size_t entrie_s = strnlen(entries->d_name, MAXLEN + 1);

		if(entrie_s < prefix_lenght) continue;

		int n = strncmp(entries->d_name, prefix, prefix_lenght);

		if(n != 0) continue;

		if(c_line >= capacity) {
			char **tmp;
			size_t new_capacity;
			new_capacity = capacity * 2;
			tmp = realloc(list, sizeof(char *) * new_capacity);

			if(tmp == NULL) {
				status = -1;
				goto out;
			}

			list = tmp;
			capacity = new_capacity;
		}

		list[c_line] = malloc(sizeof(char) * entrie_s);
		list[c_line] = strndup(entries->d_name, entrie_s);

		list[++c_line] = NULL;
	}

	if(c_line == 0) {
		free(list);
		list = NULL;
		status = 0;
		goto out;
	}

	status = 0;
out:
	free(path_to_files);
	free(prefix);
	closedir(dir);

	struct loader_entries *entrie_list;
	entrie_list = malloc(sizeof(struct loader_entries));
	entrie_list->list = list;
	entrie_list->counts = c_line;
	entrie_list->error = status;

	return entrie_list;
}
