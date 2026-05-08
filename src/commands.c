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

int show_host_info(int argc, char **argv)
{
	
	int my_root_fd;
	enum plank_status ret;
	
	struct snapshot_list root_snapshot_info;
	root_snapshot_info.list = NULL;

	char *entry_token = NULL;
	char *pretty_name = NULL;
	kernel_list host;
	host.list = NULL;
	char *host_uuid = NULL;
	char *boot_path = NULL;

	my_root_fd = open("/", O_RDONLY);

	ret = get_snapshot_list(my_root_fd, &root_snapshot_info);

	if(ret == plank_NO_SNAPSHOT_FOUND) {
		printf("no snapshot found\n");
		free(root_snapshot_info.list);
		return 0;
	}

	if(ret != plank_OK) goto out;

	for(size_t i = 0; i < root_snapshot_info.counts; i++) {

		printf(
			"snapshot id: %" PRIu64 "\n", 
			root_snapshot_info.list[i].snapshot_id);
	}
	ret = get_entry_token(&entry_token);

	printf("entry token: %s\n", entry_token);
	ret = get_value_by_key(&pretty_name, "PRETTY_NAME");

	printf("pretty name: %s\n", pretty_name);

	ret = get_kernel_list(&host);

	for(size_t i = 0; i < host.counts; i++) {
		printf("kernel version: %s found\n", host.list[i].kernel_ver);
	}
	ret = get_host_uuid(&host_uuid);
	printf("host UUID: %s\n", host_uuid);
	ret = get_boot_path(&boot_path);

	if( ret == plank_err) {
		ret = 4;
		goto out;
	}

	if(ret == plank_boot_not_found) {
		printf("$BOOT not found on its usual location:\n"
				"\t/efi /boot /boot/efi \n");
		goto out;
	}

	printf("path to $BOOT : %s\n", boot_path);

out:
	free(host.list);
	free(entry_token);
	free(pretty_name);
	free(host_uuid);
	free(boot_path);
	free(root_snapshot_info.list);
	
	return ret;
}

int make_entrie(int argc, char **argv)
{
	struct snapshot_list tar_subvol_snap_info;
	tar_subvol_snap_info.list = NULL;
	loader_entry_w *host_loader = NULL;
	kernel_list host;
	host.list = NULL;

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

	ret = get_snapshot_list(tar_subvol_fd, &tar_subvol_snap_info);

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
		fprintf(stderr, "unable to find $BOOT is it correctly mounted?\n");
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

	for(size_t i = 0; i < tar_subvol_snap_info.counts; i++) {

		struct tm t;
		char date_time[32];

		localtime_r(
			&tar_subvol_snap_info.list[i].snapshot_time.tv_sec,
			&t);

		strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", &t);

		for (size_t y = 0; y < host.counts; y++) {
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
					host.list[y].kernel_ver,
					tar_subvol_snap_info.list[i].snapshot_time.tv_sec);

			len = asprintf(&title, "snapshot %s %s %s",
					pretty_name,
					date_time,
					host.list[y].kernel_ver);

			len = asprintf(&sort_key, "%s-%s",
					id,
					host.list[y].kernel_ver);

			len = asprintf(&path_to_kernel, "/%s/%s/linux",
					entry_token,
					host.list[y].kernel_ver);

			len = asprintf(&path_to_initrd, "/%s/%s/initrd",
					entry_token,
					host.list[y].kernel_ver);

			len = asprintf(&options, "root=UUID=%s rw rootflags=subvolid=%" PRIu64 
					" loglevel=3 quiet systemd.machine_id=%s",
					host_uuid, 
					tar_subvol_snap_info.list[i].snapshot_id,
					entry_token);
			
			len = asprintf(&version, "%s",
					host.list[y].kernel_ver);



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
	
	free(tar_subvol_snap_info.list);
	free(host_loader);
	free(host.list);

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
	int tar_subvol_fd;

	kernel_list host;
	host.list = NULL;
	struct snapshot_list tar_subvol_snap_info;
	tar_subvol_snap_info.list = NULL;
	struct loader_entries *entries = NULL;

	char *entry_token = NULL;
	char *boot_path = NULL;
	char **list_to_delete = NULL;

	tar_subvol_fd = open("/", O_RDONLY);

	if(tar_subvol_fd == -1) {
		ret = -1;
		goto out;
	}

	ret = get_snapshot_list(tar_subvol_fd, &tar_subvol_snap_info);

	if(ret == plank_NO_SNAPSHOT_FOUND) {
		printf("no snapshot found\n");
		goto clean_all;
	}

	ret = get_kernel_list(&host);
	if(ret == plank_err) goto out;

	ret = get_entry_token(&entry_token);
	if(ret == plank_err) goto out;

	ret = get_boot_path(&boot_path);
	if(ret == plank_boot_not_found) {
		printf("unable to find $BOOT. is it mounted correctly ?\n");
		ret = 2;
		goto out;
	}

	if(ret == plank_err) goto out;
	entries = list_loader_entries(boot_path, entry_token);

	if(entries->counts == 0) {
		printf("No loader entrie to remove\n");
		ret = 0;
		goto out;
	}

	if(entries->status != 0) {
		printf("failed to get loader entrie list\n");
		ret = entries->status;
		goto out;
	}

	link_loader_entries(&host, &tar_subvol_snap_info, entries);

	size_t entry_del_count = 0;
	list_to_delete = malloc(sizeof(char *) * entries->counts);

	for(size_t p = 0; p < entries->counts; p++) {
		if(entries->entrie[p].status == ENTRY_DELETE_PENDING) {
			asprintf(&list_to_delete[entry_del_count++],
				"%s/loader/entries/%s",
				boot_path,
				entries->entrie[p].file_name);
		}
	}

	if(entry_del_count == 0) {
		printf("no loader entrie to remove\n");
		ret = 0;
		goto out;
	}

	printf("about ot delete following entries\n");

	for(size_t i = 0; i < entry_del_count; i++) {
		printf("%s\n", list_to_delete[i]);
	}

	printf("type y and hit enter to delete them\n");
	char input[100];
	scanf("%s", input);

	int c = strncmp(input, "y", 1);
	if(c != 0) {
		printf("Abort!!\n");
		ret = 0;
		goto out;
	}

	printf("deleting entries\n");

	for(size_t i = 0; i < entry_del_count; i++)
		unlink(list_to_delete[i]);

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

	entries = list_loader_entries(boot_path, entry_token);

	if(entries == NULL && entries->status != 0) {
		ret = entries->status;
		printf("failed to delete entries\n");
		goto out;
	}

	if(entries->counts == 0) {
		printf("No entrie to delete\n");
		ret = 0;
		goto out;
	}

	char *path_to_file = NULL;

	printf("deleting following files:\n");

	for(size_t i = 0; i < entries->counts; i++) {

	asprintf(&path_to_file, "%s/loader/entries/%s",
			boot_path,
			entries->entrie[i].file_name);

	printf("%s\n", path_to_file);
	unlink(path_to_file);

	free(path_to_file);
	}

out:
	if(tar_subvol_fd != -1) close(tar_subvol_fd);
	free(host.list);
	free(tar_subvol_snap_info.list);
	free(entry_token);
	free(boot_path);

	if(entries != NULL) {
		for(size_t i = 0; i < entries->counts; i++)
			free(entries->entrie[i].file_name);
		free(entries->entrie);
	}
	free(entries);
	if(list_to_delete != NULL)
		for(size_t i = 0; i < entry_del_count; i++)
			free(list_to_delete[i]);

	free(list_to_delete);

	return ret;
}

int show_entrie(int argc, char **argv)
{
	int status;
	struct loader_entries *entries = NULL;
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

	entries = list_loader_entries(path_to_boot, entry_token);

	if(entries->counts == 0) {
		printf("no loader entries found\n");
		status = entries->status;
		goto exit;
	}

	char *option = argv[1];

	if(strcmp(option, "-l") == 0) goto just_list;

	status = 0;

just_list:

	for (size_t i = 0; i < entries->counts; i++) {
		printf("%s\n", entries->entrie[i].file_name);
	}

	status = 0;
exit:
	free(path_to_boot);
	free(entry_token);

	if(entries != NULL) {
		for(size_t i = 0; i < entries->counts; i++)
			free(entries->entrie[i].file_name);
	}

	free(entries->entrie);
	free(entries);

	return status;
}
