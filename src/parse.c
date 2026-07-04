#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"

static char **read_file(FILE *file)
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

		if (strncmp(buffer, "#", 1) == 0)
			continue;

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

static char *split_val_key(char *line)
{
	int c = ' ';

	char *space = strchr(line, c);
	*space = '\0';

	space++;

	for(;;) {
		int p = isspace(*space);
		if (p == 0) break;

		space++;
	}

	return space;
}

struct key_val_t {
	char *key;
	char *value;
};

static struct key_val_t *get_key_val_t(char **lines)
{
	struct key_val_t *ret;
	size_t capacity = 10;
	size_t k_v_count = 0;

	ret = malloc(sizeof(struct key_val_t) * capacity);
	if (ret == NULL)
		return ret;

	for (int i = 0; lines[i] != NULL; i++) {

		if (capacity < k_v_count + 1) {
			struct key_val_t *tmp;
			size_t new_cap = capacity * 2;

			tmp = realloc(ret, sizeof(struct key_val_t) * new_cap);
			if (tmp == NULL)
				return NULL;

			ret = tmp;
			capacity = new_cap;
		}

		char *key = lines[i];
		char *val = split_val_key(lines[i]);

		ret[k_v_count].key = key;
		ret[k_v_count].value = val;

		++k_v_count;

		ret[k_v_count].key = NULL;
		ret[k_v_count].value = NULL;

	}

	return ret;
}

#define EN_TITLE	1
#define EN_VERSION	2
#define EN_MACHINE_ID	3
#define EN_SORT_KEY	4
#define EN_OPTIONS	5
#define EN_LINUX	6
#define EN_INITRD	7

static int cmp_key(const char *key)
{
	if (strcmp(key, "title") == 0)
		return EN_TITLE;

	if (strcmp(key, "version") == 0)
		return EN_VERSION;

	if (strcmp(key, "machine-id") == 0)
		return EN_MACHINE_ID;

	if (strcmp(key, "sort-key") == 0)
		return EN_SORT_KEY;

	if (strcmp(key, "options") == 0)
		return EN_OPTIONS;

	if (strcmp(key, "linux") == 0)
		return EN_LINUX;

	if (strcmp(key, "initrd") == 0)
		return EN_INITRD;

	return -1;
}

struct loader_entrie_w *parse_entrie(FILE *file)
{
	struct loader_entrie_w *ret = NULL;
	char **lines = NULL;

	struct key_val_t *tab = NULL;

	lines = read_file(file);

	if (lines == NULL) {
		ret = NULL;
		goto out;
	}

	ret = malloc(sizeof(struct loader_entrie_w));

	tab = get_key_val_t(lines);
	size_t tab_c = 0;

	while (tab[tab_c].key != NULL) {

		switch (cmp_key(tab[tab_c].key)) {
		case EN_TITLE:
			ret->title = strdup(tab[tab_c].value);
			break;

		case EN_VERSION:
			ret->version = strdup(tab[tab_c].value);
			break;

		case EN_MACHINE_ID:
			ret->machine_id = strdup(tab[tab_c].value);
			break;

		case EN_SORT_KEY:
			ret->sort_key = strdup(tab[tab_c].value);
			break;

		case EN_OPTIONS:
			ret->options = strdup(tab[tab_c].value);
			break;

		case EN_LINUX:
			ret->kernel = strdup(tab[tab_c].value);
			break;

		case EN_INITRD:
			ret->initrd = strdup(tab[tab_c].value);
			break;
		}

		++tab_c;

	}

out:
	free(tab);
	for(int i = 0; lines[i] != NULL; i++)
		free(lines[i]);

	free(lines);

	return ret;
}
