#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "types.h"
#include "parse.h"

static int parse_op_uuid(const char *op, char **ret);
static int parse_op_subvolid(char *op, char **ret);

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

struct loader_entry_w *parse_entry(FILE *fl, const char *path)
{
	FILE *file = NULL;
	char **lines = NULL;
	char *file_name = NULL;
	struct loader_entry_w *ret = NULL;
	struct key_val_t *tab = NULL;
	int should_close = 0;

	if (fl == NULL && path == NULL)
		return NULL;

	if (fl == NULL) {
		file = fopen(path, "r");
		if (file == NULL) {
			perror("parse_entry");
			goto out;
		}
		should_close = 1;
	} else {
		file = fl;
	}

	if (path == NULL)
		goto read_file;

	file_name = (char *) bsname(path);
	file_name = strdup(file_name);
	if (file_name == NULL)
		goto out;

read_file:
	lines = read_file(file);
	if (lines == NULL) {
		ret = NULL;
		goto out;
	}

	ret = malloc(sizeof(struct loader_entry_w));
	if (ret == NULL)
		goto out;

	tab = get_key_val_t(lines);
	if (tab == NULL) {
		free(ret);
		ret = NULL;
		goto out;
	}

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

	ret->filename = file_name;
out:
	if (should_close)
		fclose(fl);

	free(tab);
	for(int i = 0; lines[i] != NULL; i++)
		free(lines[i]);

	free(lines);

	return ret;
}

int parse_option(const char *op, int target, char **ret)
{
	if (op == NULL || ret == NULL)
		return -1;

	/**
	 * lets dont overwrite caller string
	 */
	int status = 0;
	char *op_d = strdup(op);
	if (op_d == NULL) {
		perror("parse_option(strdup)");
		status = -1;
		goto fail;
	}

	switch (target) {
	case OP_UUID:
		status = parse_op_uuid(op_d, ret);
		break;

	case OP_SUBVOL_ID:
		status = parse_op_subvolid(op_d, ret);
		break;

	default:
		fprintf(stderr,
			"parse_option: unknown '%d' target passed", target);
		status = -1;
	}

	if (status)
		goto fail;

	free(op_d);
	return status;
fail:
	fprintf(stderr,
		"parse_option: failed to parse option\n");

	free(op_d);
	return status;
}

static int parse_op_uuid(const char *op, char **ret)
{
	char *uuid_str = strstr(op, "UUID=");
	if (!uuid_str)
		goto fail;

	char *start = strchr(uuid_str, '=') + 1;
	char *uuid = malloc(sizeof(char) * 37);
	if (!uuid)
		goto fail;

	strncpy(uuid, start, 36);
	*ret = uuid;

	return 0;
fail:
	fprintf(stderr, "parse_op_uuid: failed to parse uuid\n");
	fprintf(stderr, "parse_op_uuid: following string passed: %s\n", op);
	return -1;
}

static int parse_op_subvolid(char *op, char **ret)
{
	char *subvolid_str = strstr(op, "subvolid=");
	if (!subvolid_str)
		goto fail;

	char *start = subvolid_str + strlen("subvolid=");

	char *end = strchr(start, ' ');
	if (!end)
		goto fail;

	*end = '\0';
	size_t len = strlen(start) + 1;

	char *subvolid = malloc(sizeof(char) * len);
	if (!subvolid)
		goto fail;

	strncpy(subvolid, start, len);

	*ret = subvolid;
	return 0;
fail:
	fprintf(stderr, "parse_op_subvolid: failed to parse subvolid\n");
	fprintf(stderr, "parse_op_subvolid: following string passed: %s\n", op);
	return -1;
}

int parse_title(const char *title, int target, char **ret)
{
	char *pretty_name = NULL;
	char *time_str = NULL;
	char *kern_ver_str = NULL;
	char *dup_title = NULL;
	char *ret_str = NULL;

	if (title == NULL || ret == NULL)
		goto inval;

	dup_title = strdup(title);
	if (!dup_title)
		goto strdup_fail;
	/**
	 * first call will just point 'snapshot' whcih we
	 * really dont want to save
	 */
	strtok(dup_title, " ");

	pretty_name = strtok(NULL, " ");
	time_str = strtok(NULL, " ");
	/**
	 * 'jn' will point to next part of same time string.
	 * it assume time is written like this.
	 * 	2026-08-02 00:05:17
	 * above call may have turn that spcae into NULL byte
	 * so after having pointer to nexk token, in this case
	 * its '00:05:17'. then we can turn that NULL byte into
	 * space again.
	 */
	char *jn = strtok(NULL, " ");
	*(jn - 1) = ' ';

	kern_ver_str = strtok(NULL, " ");

	switch (target) {
	case TITLE_PRETTY_NAME:
		ret_str = strdup(pretty_name);
		break;

	case TITLE_KERN_VER:
		ret_str = strdup(kern_ver_str);
		break;

	case TITLE_SNAP_TIME:
		ret_str = strdup(time_str);
		break;

	default:
		goto un_tar;
	}
	if (!ret_str)
		goto strdup_fail;

	*ret = ret_str;

	free(dup_title);
	return 0;

un_tar:
	fprintf(stderr,
		"parse_title: unknown target '%d' passed\n", target);
	goto fail;
inval:
	fprintf(stderr,
		"parse_title: invalid argument passed(null pointer)\n");
	goto fail;

strdup_fail:
	perror("parse_title(strdup)");
	goto fail;

fail:
	fprintf(stderr,
		"parse_title: failed to parse title\n");
	free(dup_title);
	return -1;
}
/**
 * FIX ME: 'parse_id_skey' require review and refactor as it
 * 	   for now use pointer arthemetic to find size of id.
 *
 * prase id from sort-key string
 */
int parse_id_skey(char **ret_str, const char *sort_key)
{
	char *id = NULL;
	char *end = NULL;
	size_t len = 0;

	end = strchr(sort_key, '-');

	len = end - sort_key;

	id = malloc(sizeof(char) * len);
	if (!id)
		return -1;

	strncpy(id, sort_key, len);

	*ret_str = id;
	return 0;
}
/**
 * example string str-> "/home/harry/file.c"
 */
const char *bsname(const char *path)
{
	const char *ret = NULL;
	char *slash = NULL;

keep_finding:
	for (;;) {
		if (*path == '/')
			break;
		if (*path == '\0')
			goto got_it;

		path++;
	}

	slash = (char *) path;

	if (*path != '\0') {
		path++;
		goto keep_finding;
	} else {
got_it:
		ret = slash + 1;
	}

	return ret;
}
