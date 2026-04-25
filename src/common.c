#include <dirent.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libmount/libmount.h>
#include <blkid/blkid.h>
#include "btrfs.h"

static int find_ld_etr(int fd, unsigned int depth_count) {

	if(depth_count > 2) return 1;

	DIR *loader = fdopendir(fd);
	struct dirent *dt;

	while((dt = readdir(loader)) != NULL) {

		int c = strcmp(dt->d_name, "loader");

		if(c == 0) {
			int d = openat(fd, "loader", O_RDONLY);
			close(fd);
			closedir(loader);
			
			return find_ld_etr(d, depth_count + 1);
		}

		c = strcmp(dt->d_name, "entries");

		if(c == 0) {
			close(fd);
			closedir(loader);

			return 0;
		}

	}

	close(fd);
	closedir(loader);

	return 1;
}

enum plank_status get_entry_token(char **ret)
{
	enum plank_status f_ret = plank_OK;

	FILE *machine_id = fopen("/etc/machine-id", "r");
	int c;
	size_t n = 0;
	char buffer[1024];

	while((c = fgetc(machine_id)) != EOF) {
		if(memcmp(&c, "\n", 1) == 0) continue;

		buffer[n++] = (char)c;
		buffer[n] = 0;

		if(n > 1024) {
			f_ret = plank_err;
			goto out;
		}
	}

	 *ret = malloc(strlen(buffer) + 1);

	 if(*ret == NULL) {
		 f_ret = plank_err;
		 goto out;
	 }

	strcpy(*ret, buffer);

out:

	fclose(machine_id);
	return f_ret;
}

enum plank_status get_value_by_key(char **ret, char *target)
{
	FILE *os_release = fopen("/etc/os-release", "r");
	int c;
	size_t n = 0;
	size_t len = 0;
	char buffer[1024];
	char key[1024];
	char value[1024];

	while((c = fgetc(os_release)) != EOF) {
		if(memcmp(&c, "\"", 1) == 0) continue;

		if(memcmp(&c, "\'", 1) == 0) continue;

		if(memcmp(&c, "=", 1) == 0) {
			strcpy(key, buffer);
			n = 0;
			continue;
		}

		if(memcmp(&c, "\n", 1) == 0) {
			strcpy(value, buffer);
			n = 0;
			len = strlen(value);
			int str;
			str = strcmp(key, target);
			if (str == 0) goto found_it;

			continue;
			
		}

		buffer[n++] = (char)c;
		buffer[n] = 0;
	}

found_it:

	*ret = malloc(len + 1);
	strcpy(*ret, value);
	fclose(os_release);
	return plank_OK;

}

enum plank_status get_host_uuid(char **ret)
{
	enum plank_status f_ret;
	struct libmnt_table *tb = mnt_new_table();
	struct libmnt_fs *fs = NULL;
	blkid_probe pr = NULL;

	f_ret = mnt_table_parse_mtab(tb, NULL);

	if(f_ret < 0) goto out;

	const char *target_mountpoint = "/";

	fs = mnt_table_find_target(
		tb, target_mountpoint,
		MNT_ITER_BACKWARD);

	if( fs == NULL) {
		f_ret = plank_err;
		goto out;
	}

	const char *devname = mnt_fs_get_srcpath(fs);

	if(devname == NULL) {
		f_ret = plank_err;
		goto out;
	}

	pr = blkid_new_probe_from_filename(devname);

	if(pr == NULL) {
		f_ret = plank_err;
		goto out;
	}

	int ret_tmp;

	ret_tmp = blkid_do_safeprobe(pr);

	if(ret_tmp != 0) {
		f_ret = plank_err;
		goto out;
	}

	const char *target_uuid = NULL;

	ret_tmp = blkid_probe_lookup_value(
		pr, "UUID", &target_uuid, NULL);

	if(ret_tmp == -1) {
		f_ret = plank_err;
		goto out;
	}

	*ret = malloc((strlen(target_uuid) + 1));

	strcpy(*ret, target_uuid);

out:
	mnt_unref_table(tb);
	blkid_free_probe(pr);

	return f_ret;
}

enum plank_status get_boot_path(char **ret)
{
	const char *path_to_look[] = {"/boot", "/boot/efi", "/efi"};

	int e;
	for(e = 0; e < 3; e++) {
		int o = open(path_to_look[e], O_RDONLY);
		if(o == -1) continue;

		int p = find_ld_etr(o, 0);

		if(p == 0) {

			*ret = malloc(strlen(path_to_look[e]) + 1);
			strcpy(*ret, path_to_look[e]);
			return plank_OK;
		}

		if(p == 1) continue;

	}

	return plank_boot_not_found;
}

enum plank_status get_ker_ver_snap_tim(
	char *file_name,
	char *kernel_ver,
	struct timespec *tm)
{
	enum plank_status ret = plank_OK;
	char *target = strdup(file_name);
	char *machine_id_start = strchr(target, '-') + 1;
	char *kernel_ver_start = strchr(machine_id_start, '-') + 1;
	char *snap_time_start = strrchr(target, '-') + 1;
	char *kernel_ver_nul = snap_time_start - 1;
	*kernel_ver_nul = '\0';

	size_t kernel_ver_len = strlen(kernel_ver_start);

	strncpy(kernel_ver, kernel_ver_start, kernel_ver_len + 1);

	tm->tv_sec = atol(snap_time_start);
	tm->tv_nsec = 0;

out:
	free(target);

	return ret;
}
