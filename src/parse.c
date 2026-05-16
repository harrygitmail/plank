#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"

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
