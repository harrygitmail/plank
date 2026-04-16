#include <dirent.h>
#include <fcntl.h>
#include <stddef.h>
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
	struct libmnt_table *tb = mnt_new_table();
	struct libmnt_fs *fs;

	int c = mnt_table_parse_mtab(tb, NULL);

	if(c < 0) return plank_err;

	const char *target_mountpoint = "/";

	fs = mnt_table_find_target(tb, target_mountpoint, MNT_ITER_BACKWARD);

	if( fs == NULL) return plank_err;

	const char *devname = mnt_fs_get_srcpath(fs);

	if(devname == NULL) return plank_err;

	blkid_probe pr = blkid_new_probe_from_filename(devname);

	if(pr == NULL) return plank_err;

	c = blkid_do_safeprobe(pr);

	if(c != 0) return plank_err;

	const char *target_uuid = NULL;

	c = blkid_probe_lookup_value(pr, "UUID", &target_uuid, NULL);

	if(c == -1) return plank_err;

	*ret = malloc((strlen(target_uuid) + 1));

	strcpy(*ret, target_uuid);

	return plank_OK;
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
