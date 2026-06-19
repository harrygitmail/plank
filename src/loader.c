#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include "types.h"
#include "common.h"

struct loader_entries *list_loader_entries(const char *BOOT,
	const char *entry_token)
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
	const struct kernel_list *kern_list,
	const struct snapshot_list *snap_list,
	struct loader_entries *const entries)
{
	size_t entrie_c = 0;

	struct snapshot_info temp_snap_struct;
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

				struct snapshot_info *this;
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

#define ON_ERR_OUT(X, Y) 		\
	if ((X) == -1 ) {		\
		ret = (Y);		\
		goto out;		\
	}


enum plank_status pre_entrie(struct system_info *s_info,
	struct loader_entrie_w **entrie_out)
{
	struct snapshot_list snap;
	snap = s_info->system.snap;

	struct kernel_list kern;
	kern = s_info->system.kern;

	int len;
	enum plank_status ret = PLANK_OK;

	size_t capacity = 5;
	size_t n = 0;

	*entrie_out = malloc(sizeof(struct loader_entrie_w) * capacity);

	for (size_t i = 0; i < snap.counts; i++) {
		struct tm t;
		char date_time[32];

		struct timespec snap_time;
		snap_time = snap.list[i].snapshot_time;

		localtime_r(&snap_time.tv_sec, &t);

		strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", &t);

		for (size_t j = 0; j < kern.counts; j++) {

			if (n >= capacity + 1) {
				struct loader_entrie_w *tmp;
				size_t new_capacity = capacity * 2;

				size_t nsize;
				nsize = sizeof(struct loader_entrie_w) * new_capacity;

				tmp = realloc(*entrie_out, nsize);

				if (tmp == NULL)
					goto out;

				*entrie_out = tmp;
				capacity = new_capacity;

			}

			char *filename = NULL;
			char *title = NULL;
			char *sort_key = NULL;
			char *path_to_kernel = NULL;
			char *path_to_initrd = NULL;
			char *options = NULL;
			char *version = NULL;

			len = asprintf(&filename, "snapshot-%s-%s-%ld.conf",
				s_info->system.eb.entry_token,
				kern.list[j].kernel_ver,
				snap.list[i].snapshot_time.tv_sec);

			ON_ERR_OUT(len, PLANK_ERR);

			len = asprintf(&title, "snapshot %s %s %s",
				s_info->os_release.pretty_name,
				date_time,
				kern.list[j].kernel_ver);

			ON_ERR_OUT(len, PLANK_ERR);

			len = asprintf(&sort_key, "%s-%s",
				s_info->os_release.id,
				kern.list[j].kernel_ver);

			ON_ERR_OUT(len, PLANK_ERR);

			len = asprintf(&path_to_kernel, "/%s/%s/linux",
				s_info->system.eb.entry_token,
				kern.list[j].kernel_ver);

			ON_ERR_OUT(len, PLANK_ERR);

			len = asprintf(&path_to_initrd, "/%s/%s/initrd",
				s_info->system.eb.entry_token,
				kern.list[j].kernel_ver);

			ON_ERR_OUT(len, PLANK_ERR);

			len = asprintf(&options,
				"root=UUID=%s rw rootflags=subvolid=%" PRIu64
				" loglevel=3 quiet systemd.machine_id=%s",
				s_info->system.mount_info.uuid,
				snap.list[i].snapshot_id,
				s_info->system.eb.entry_token);

			ON_ERR_OUT(len, PLANK_ERR);

			len = asprintf(&version, "%s",
				kern.list[j].kernel_ver);

			ON_ERR_OUT(len, PLANK_ERR);

			(*entrie_out)[n].filename = filename;
			(*entrie_out)[n].title = title;
			(*entrie_out)[n].version = version;
			(*entrie_out)[n].machine_id = s_info->system.eb.entry_token;
			(*entrie_out)[n].sort_key = sort_key;
			(*entrie_out)[n].options = options;
			(*entrie_out)[n].kernel = path_to_kernel;
			(*entrie_out)[n].initrd = path_to_initrd;

			n++;

			(*entrie_out)[n].filename = NULL;


		}

	}
out:

	return ret;
}
