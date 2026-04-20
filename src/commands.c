#define _GNU_SOURCE 
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include "btrfs.h"
#include "common.h"
#include "kernel.h"
#include "loader.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

int show_host_info(int argc, char **argv)
{
	
	int my_root_fd;
	my_root_fd = open("/", O_RDONLY);

	snapshot_info *my_root_snapshot = NULL;

	enum plank_status ret;

	ret = get_snapshot_list(my_root_fd, &my_root_snapshot);

	if(ret == plank_NO_SNAPSHOT_FOUND) {
		printf("no snapshot found\n");
		free(my_root_snapshot);
		return 0;
	}

	if(ret != plank_OK) {
		free(my_root_snapshot);
		return 1;
	}

	for(int i = 0; ; i++) {
		if(my_root_snapshot[i].snapshot_id == 0)
			break;


		printf("snapshot id: %" PRIu64 "\n", my_root_snapshot[i].snapshot_id);
	}

	char *entry_token = NULL;
	ret = get_entry_token(&entry_token);

	printf("entry token: %s\n", entry_token);

	free(entry_token);

	char *pretty_name = NULL;

	ret = get_value_by_key(&pretty_name, "PRETTY_NAME");

	printf("pretty name: %s\n", pretty_name);

	free(pretty_name);

	kernel_list *host = NULL;

	ret = get_kernel_list(&host);

	for(int i = 0; ; i++) {
		if(memcmp(host[i].kernel_ver, "STOP", 4) == 0) break;
		
		printf("kernel version: %s found\n", host[i].kernel_ver);
	}

	free(host);

	char *host_uuid = NULL;

	ret = get_host_uuid(&host_uuid);
	printf("host UUID: %s\n", host_uuid);
	
	free(host_uuid);

	char *boot_path = NULL;
	ret = get_boot_path(&boot_path);

	if( ret == plank_err) return 4;

	if(ret == plank_boot_not_found) {
		printf("$BOOT not found on its usual location:\n"
				" /efi /boot /boot/efi ");
		return 0 ;
	}

	printf("path to $BOOT : %s\n", boot_path);
	free(boot_path);
	
	return 0;
}

int make_entry(int argc, char **argv)
{
	snapshot_info *tar_subvol = NULL;
	loader_entry_w *host_loader = NULL;
	kernel_list *host = NULL;

	char *pretty_name = NULL;
	char *entry_token = NULL;
	char *boot_path = NULL;
	char *host_uuid = NULL;
	char *id = NULL;
	const char *entry_type = "# Boot Loader Specification type #1 entry";
	const char *warning = "# File created by 'plank' and may not work";
	

	enum plank_status ret;

	int len;

	int tar_subvol_fd;
	tar_subvol_fd = open("/", O_RDONLY);

	if( tar_subvol_fd == -1 ) {
		ret = -1;
		goto out;
	}

	ret = get_snapshot_list(tar_subvol_fd, &tar_subvol);

	if(ret == plank_NO_SNAPSHOT_FOUND) {
		printf("NO snapshot found\n");
		ret = 0;
		goto out;
	}

	if(ret != plank_OK) goto out;

	ret = get_kernel_list(&host);

	if(ret != plank_OK) goto out;
	
	ret = get_entry_token(&entry_token);
	if(ret == plank_err) goto out;

	ret = get_boot_path(&boot_path);

	if(ret == plank_boot_not_found) {
		fprintf(stderr, "unable to find $BOOT is it correctly mounted?");
		goto out;
	}

	ret = get_host_uuid(&host_uuid);

	if(ret == plank_err) goto out;

	/* lets use default values if parsing os-release gives
	 * any error.
	 */

	ret = get_value_by_key(&pretty_name, "PRETTY_NAME");
	if(ret == plank_err) pretty_name = strdup("Linux");

	ret = get_value_by_key(&id, "ID");
	if(ret == plank_err) id = strdup("linux");
	
	
	size_t capacity = 6;
	size_t n = 0;

	host_loader = malloc(sizeof(loader_entry_w) * capacity);

	for(int i = 0; ; i++) {
		if(tar_subvol[i].snapshot_id == 0) break;

		struct tm t;
		char date_time[32];

		localtime_r(&tar_subvol[i].snapshot_time.tv_sec, &t);
		strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", &t);

		for (int y = 0; ; y++) {
			if(strncmp(host[y].kernel_ver, "STOP", 4) == 0) break;

			if(n > capacity) {
				size_t new_capacity = capacity * 2;
				loader_entry_w *tmp = realloc(host_loader, new_capacity);

				if(tmp == NULL) continue;

				host_loader = tmp;
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
					entry_token, 
					host[y].kernel_ver, 
					tar_subvol[i].snapshot_time.tv_sec);

			len = asprintf(&title, "snapshot %s %s %s",
					pretty_name,
					date_time,
					host[y].kernel_ver);

			len = asprintf(&sort_key, "%s-%s",
					id,
					host[y].kernel_ver);

			len = asprintf(&path_to_kernel, "/%s/%s/linux",
					entry_token,
					host[y].kernel_ver);

			len = asprintf(&path_to_initrd, "/%s/%s/initrd",
					entry_token,
					host[y].kernel_ver);

			len = asprintf(&options, "root=UUID=%s rw rootflags=subvolid=%" PRIu64 
					" loglevel=3 quiet systemd.machine_id=%s",
					host_uuid, 
					tar_subvol[i].snapshot_id,
					entry_token);
			
			len = asprintf(&version, "%s",
					host[y].kernel_ver);



			if(len == -1) {
				ret = plank_err;
				goto clean_up_temp_fel;
			}

			char *f_filename = NULL;
			char *f_title = NULL;
			char *f_version = NULL;
			char *f_machine_id = NULL;
			char *f_sort_key = NULL;
			char *f_linux = NULL;
			char *f_initrd = NULL;
			char *f_options = NULL;

			len = asprintf(&f_filename, "%s",
					filename);

			len = asprintf(&f_title,"title      %s",
					title);
			
			len = asprintf(&f_version, "version    %s",
					version);

			len = asprintf(&f_machine_id, "machine-id %s",
					entry_token);

			len = asprintf(&f_sort_key, "sort-key   %s",
					sort_key);

			len = asprintf(&f_linux, "linux      %s",
					path_to_kernel);

			len = asprintf(&f_initrd, "initrd     %s",
					path_to_initrd);

			len = asprintf(&f_options, "options    %s",
					options);

clean_up_temp_fel:
			free(filename);
			free(title);
			free(sort_key);
			free(path_to_kernel);
			free(path_to_initrd);
			free(options);
			free(version);

			if(len == -1) {
				ret = plank_err;
				continue;
			}

			host_loader[n].filename = f_filename;
			host_loader[n].title = f_title;
			host_loader[n].version = f_version;
			host_loader[n].machine_id = f_machine_id;
			host_loader[n].sort_key = f_sort_key;
			host_loader[n].kernel = f_linux;
			host_loader[n].initrd = f_initrd;
			host_loader[n].options = f_options;

			n++;
			host_loader[n].filename = NULL;

		}
	}

	int p;
	
	for(p = 0; ; p++) {
		if(host_loader[p].filename == NULL) break;

		char *path_to_file = NULL;

		len = asprintf(&path_to_file, "%s/loader/entries/%s",
				boot_path,
				host_loader[p].filename);

		if(len == -1) goto write_out;

	
		FILE *file = fopen(path_to_file, "w");

		fwrite(entry_type, strlen(entry_type), 1, file);

		fwrite("\n", 1, 1, file);

		fwrite(warning, strlen(warning), 1, file);

		fwrite("\n", 1, 1, file);
		
		fwrite(host_loader[p].title,
				strlen(host_loader[p].title), 1, file);

		fwrite("\n", 1, 1, file);

		fwrite(host_loader[p].version, 
				strlen(host_loader[p].version),
				1,
				file);

		fwrite("\n", 1, 1, file);

		fwrite(host_loader[p].machine_id, 
				strlen(host_loader[p].machine_id),
				1,
				file);

		fwrite("\n", 1, 1, file);
	
		fwrite(host_loader[p].sort_key, 
				strlen(host_loader[p].sort_key),
				1, 
				file);

		fwrite("\n", 1, 1, file);

		fwrite(host_loader[p].kernel,
				strlen(host_loader[p].kernel),
				1,
				file);

		fwrite("\n", 1, 1, file);

		fwrite(host_loader[p].initrd,
				strlen(host_loader[p].initrd),
				1,
				file);

		fwrite("\n", 1, 1, file);

		fwrite(host_loader[p].options,
				strlen(host_loader[p].options),
				1,
				file);

		fwrite("\n", 1, 1, file);

write_out:
		free(path_to_file);
		fclose(file);

		free(host_loader[p].filename);
		free(host_loader[p].title);
		free(host_loader[p].version);
		free(host_loader[p].machine_id);
		free(host_loader[p].sort_key);
		free(host_loader[p].kernel);
		free(host_loader[p].initrd);
		free(host_loader[p].options);
		
	}
out:
	
	free(tar_subvol);
	free(host_loader);
	free(host);

	free(boot_path);
	free(pretty_name);
	free(id);
	free(entry_token);
	free(host_uuid);

	if(tar_subvol_fd != -1) close(tar_subvol_fd);

	return ret;
}

int clean(int argc , char **argv) {
	enum plank_status ret;
	int len;
	int tar_subvol_fd;

	kernel_list *host = NULL;
	snapshot_info *tar_subvol_snapshot = NULL;

	char *entry_token = NULL;
	char *boot_path = NULL;

	tar_subvol_fd = open("/", O_RDONLY);

	if(tar_subvol_fd == -1) {
		ret = -1;
		goto out;
	}

	ret = get_snapshot_list(tar_subvol_fd, &tar_subvol_snapshot);

	if(ret == plank_NO_SNAPSHOT_FOUND) {
		printf("no snapshot found\n");
		goto clean_all;
	}

	ret = get_kernel_list(&host);
	if(ret == plank_err) goto out;

	goto out;

clean_all:

	ret = get_entry_token(&entry_token);
	if(ret == plank_err) goto out;

	ret = get_boot_path(&boot_path);
	if(ret == plank_boot_not_found) {
		printf("unable to find $BOOT. is it mounted correctly ?\n");
		goto out;
	}

	if(ret == plank_err) goto out;

	DIR *entries = NULL;
	struct dirent *entrie = NULL;	
	char *path_to_entries = NULL;
	char *file_name_to_compare = NULL;
	char **list_to_delete = NULL;
	
	len = asprintf(&path_to_entries, "%s/loader/entries/", boot_path);
	if(len == -1) {
		ret = -1;
		goto clean_all_out;
	}
	
	len = asprintf(&file_name_to_compare, "snapshot-%s", entry_token);
	if(len == -1 ) {
		ret = -1;
		goto clean_all_out;
	}

	errno = 0;

	entries = opendir(path_to_entries);

	if(entries == NULL) {
		printf("problem in opening %s\n", path_to_entries);
		ret = 1;
		goto clean_all_out;
	}

	size_t capacity = 6;
	size_t n = 0;
	size_t fnc_len = strlen(file_name_to_compare);

	list_to_delete = malloc(sizeof(char *) * capacity);
	list_to_delete[n] = NULL;

	errno = 0;

	while((entrie = readdir(entries)) != NULL) {

		if(n > capacity) {
			size_t new_capacity = capacity * 2;
			char *tmp = realloc(list_to_delete, new_capacity);
			if(tmp == NULL) goto clean_all_out;

			list_to_delete = &tmp;
			capacity = new_capacity;
		}

		int cmp;
		cmp = strncmp(entrie->d_name, ".", 1);

		if(cmp == 0) continue;

		cmp = strncmp(entrie->d_name, file_name_to_compare, fnc_len);

		if (cmp == 0) {
			char *tmp = strdup(entrie->d_name);
			if(tmp == NULL) goto clean_all_out;

			list_to_delete[n++] = tmp;

			list_to_delete[n] = NULL;
		}

	}


	if(n == 0) {
		printf("no file to remove\n");
		ret = 0;
		goto clean_all_out;
	}

	for(int i = 0; list_to_delete[i] != NULL; i++) {
		printf("file to remove %s/loader/entries/%s\n",
				boot_path,
				list_to_delete[i]);

	}


clean_all_out:

	closedir(entries);

	free(file_name_to_compare);
	free(path_to_entries);

	for(int i = 0; list_to_delete[i] != NULL; i++) free(list_to_delete[i]);
	
	free(list_to_delete);	


out:
	if(tar_subvol_fd != -1) close(tar_subvol_fd);
	free(host);
	free(tar_subvol_snapshot);
	free(entry_token);
	free(boot_path);
	return ret;
}

int show_entry(int argc, char **argv)
{
	int status;
	char **list = NULL;
	char *path_to_boot = NULL;
	char *entry_token = NULL;

	enum plank_status ret;
	ret = get_entry_token(&entry_token);

	if(ret != plank_OK) {
		status = 2;
		goto exit;
	}

	ret = get_boot_path(&path_to_boot);
	
	if(ret != plank_OK) {
		status = 2;
		goto exit;
	}

	list = list_loader_files(path_to_boot, entry_token);
	if(list == NULL) {
		status = 2;
		goto exit;
	}

	char *target = argv[1];

	if(strcmp(target, "-l") == 0) goto just_list;

	status = 0;

just_list:

	for (int i = 0;list[i] != NULL; i++) {
		printf("%s\n", list[i]);
	}

	status = 0;
exit:
	free(path_to_boot);
	free(entry_token);

	for(int i = 0; list[i] != NULL; i++) free(list[i]);

	free(list);

	return status;
}
