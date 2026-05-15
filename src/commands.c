#define _GNU_SOURCE 
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include "common.h"
#include "kernel.h"
#include "loader.h"
#include "plank.h"
#include "mount.h"
#include "types.h"
#include "btrfs.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

int show_host_info(int argc, char **argv)
{
	struct system_info info;
	int status = 0;

	info = get_system_info(SYS_INFO_SHOW);

	if (info.status == PLANK_ERR) {
		fprintf(stderr, "something went wrong!!\n");
		status = info.status;
		goto out;
	}

	if (info.status == PLANK_MNT_ERR) {
		fprintf(stderr, "libmount operation failed!\n");
		status = info.status;
		goto out;
	}

	if (info.status == PLANK_BOOT_NOT_FOUND) {
		fprintf(stderr, "Unable to find $BOOT. is it mounted corectly ?\n");
		status = info.status;
		goto out;
	}

	if (info.status == PLANK_BTRFS_NO_SNAPSHOT_FOUND) {
		fprintf(stdout, "NO snapshot found\n\n");
		goto show_kern;
	}

	printf("following snapshot found\n");

	for (size_t i = 0; i < info.system.snap.counts; i++) {
		printf("\t%" PRIu64 "\n", info.system.snap.list[i].snapshot_id);
	}

	printf("\n");

show_kern:

	if (info.system.kern.counts == 0) {
		fprintf(stdout, "NO kernel found\n\n");
		goto show_next;
	}

	printf("-----------------------------------------------\n");
	printf("following kernel found\n");

	for (size_t i = 0; i < info.system.kern.counts; i++) {
		printf("\t%s\n", info.system.kern.list[i].kernel_ver);
	}

	printf("\n");


show_next:

	printf("----------------------------------------------\n");
	printf("system info:\n\n");

	printf("entry-token: 	%s\n", info.system.entry_token);
	printf("$BOOT: 		%s\n", info.system.boot_path);
	printf("pretty name:	%s\n", info.os_release.pretty_name);
	printf("ID: 		%s\n", info.os_release.id);
	printf("UUID: 		%s", info.system.mount_info.uuid);

	
out:
	free_system_info(info, SYS_INFO_SHOW);
	return status;
}

int make_entrie(int argc, char **argv)
{
	const char *entry_type = "# Boot Loader Specification type #1 entry";
	const char *warning = "# File created by 'plank' and may not work";
	
	int len;
	int status = 0;

	struct system_info info;
	loader_entry_w *host_loader = NULL;

	info = get_system_info(SYS_INFO_WRITE);

	if (info.status == PLANK_ERR) {
		fprintf(stderr, "something went wrong!!\n");
		status = info.status;
		goto out;
	}

	if (info.status == PLANK_MNT_ERR) {
		fprintf(stderr, "libmount operation failed!\n");
		status = info.status;
		goto out;
	}

	if (info.status == PLANK_BOOT_NOT_FOUND) {
		fprintf(stderr, "Unable to find $BOOT. is it mounted corectly ?\n");
		status = info.status;
		goto out;
	}

	if (info.status == PLANK_BTRFS_NO_SNAPSHOT_FOUND) {
		fprintf(stdout, "NO snapshot found\n\n");
		goto out;
	}
	size_t capacity = 6;
	size_t n = 0;

	host_loader = malloc(sizeof(loader_entry_w) * capacity);

	for(size_t i = 0; i < info.system.snap.counts; i++) {

		struct tm t;
		char date_time[32];

		localtime_r(
			&info.system.snap.list[i].snapshot_time.tv_sec,
			&t);

		strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", &t);

		for (size_t y = 0; y < info.system.kern.counts; y++) {
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
					info.system.entry_token,
					info.system.kern.list[y].kernel_ver,
					info.system.snap.list[i].snapshot_time.tv_sec);

			len = asprintf(&title, "snapshot %s %s %s",
					info.os_release.pretty_name,
					date_time,
					info.system.kern.list[y].kernel_ver);

			len = asprintf(&sort_key, "%s-%s",
					info.os_release.id,
					info.system.kern.list[y].kernel_ver);

			len = asprintf(&path_to_kernel, "/%s/%s/linux",
					info.system.entry_token,
					info.system.kern.list[y].kernel_ver);

			len = asprintf(&path_to_initrd, "/%s/%s/initrd",
					info.system.entry_token,
					info.system.kern.list[y].kernel_ver);

			len = asprintf(&options, "root=UUID=%s rw rootflags=subvolid=%" PRIu64
					" loglevel=3 quiet systemd.machine_id=%s",
					info.system.mount_info.uuid,
					info.system.snap.list[i].snapshot_id,
					info.system.entry_token);
			
			len = asprintf(&version, "%s",
					info.system.kern.list[y].kernel_ver);



			if(len == -1) {
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
					info.system.entry_token);

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
				info.system.boot_path,
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
	free_system_info(info, SYS_INFO_WRITE);
	free(host_loader);

	return status;
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

	if(ret == PLANK_BTRFS_NO_SNAPSHOT_FOUND) {
		printf("no snapshot found\n");
		goto clean_all;
	}

	ret = get_kernel_list(&host, "/");
	if(ret == PLANK_ERR) goto out;

	ret = get_entry_token(&entry_token);
	if(ret == PLANK_ERR) goto out;

	ret = get_boot_path(&boot_path);
	if(ret == PLANK_BOOT_NOT_FOUND) {
		printf("unable to find $BOOT. is it mounted correctly ?\n");
		ret = 2;
		goto out;
	}

	if(ret == PLANK_ERR) goto out;
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
	if(ret == PLANK_ERR) goto out;

	ret = get_boot_path(&boot_path);
	if(ret == PLANK_BOOT_NOT_FOUND) {
		printf("unable to find $BOOT. is it mounted correctly ?\n");
		goto out;
	}

	if(ret == PLANK_ERR) goto out;

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

	if(ret != PLANK_OK) {
		status = 2;
		goto exit;
	}

	ret = get_boot_path(&path_to_boot);
	
	if(ret != PLANK_OK) {
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

int check(int argc, char **argv)
{
	struct system_info info;
	int status = 0;
	char **subvol_paths = NULL;
	char **snap_relative_path = NULL;
	kernel_list **kern_list_array = NULL;

	info = get_system_info(SYS_INFO_SHOW);

	if (info.status == PLANK_ERR) {
		fprintf(stderr, "something went wrong!!\n");
		status = info.status;
		goto out;
	}

	if (info.status == PLANK_LIBMOUNT_ERR) {
		fprintf(stderr, "libmount operation failed!\n");
		status = info.status;
		goto out;
	}

	if (info.status == PLANK_BOOT_NOT_FOUND) {
		fprintf(stderr, "Unable to find $BOOT. is it mounted corectly ?\n");
		status = info.status;
		goto out;
	}

	if (info.status == PLANK_BTRFS_NO_SNAPSHOT_FOUND) {
		fprintf(stdout, "NO snapshot found\n\n");
		goto out;
	}

	enum plank_status ret ;

	ret = mount_top_subvol(info.system.mount_info, "/mnt");

	if (ret != PLANK_OK) {
		fprintf(stderr, "something went wrong");
		goto out;
	}

	size_t capacity = info.system.snap.counts;

	subvol_paths = malloc(sizeof(char *) * capacity);
	kern_list_array = malloc(sizeof(kernel_list *) * capacity);
	snap_relative_path = malloc(sizeof(char *) * capacity);

	int fd = open("/mnt", O_RDONLY);
	if (fd < 0) goto out;

	for (size_t i = 0; i < info.system.snap.counts; i++) {
		kern_list_array[i] = malloc(sizeof(kernel_list));

		snap_relative_path[i] = get_subvol_path(
			info.system.snap.list[i].snapshot_id,
			fd);

		asprintf(&subvol_paths[i], "/mnt/%s",
			snap_relative_path[i]);

		get_kernel_list(kern_list_array[i], subvol_paths[i]);

	}

	for (size_t i = 0; i < capacity; i++) {
		printf("snapshot id: %" PRIu64 "\n",
			info.system.snap.list[i].snapshot_id);

		printf("snapshot path: %s\n",
			snap_relative_path[i]);

		kernel_list *k = NULL;

		k = kernel_diff(&info.system.kern, kern_list_array[i]);

		if (k->list == NULL) {
			printf("No kernel mistmatch found for this snapshot\n");
			printf("\n");
			free(k);
			continue;
		}

		printf("kernel mismatch found !!\n");
		printf("missing kernel:\n");

		for (size_t j = 0; j < k->counts; j++)
			printf("\t%s\n", k->list[j].kernel_ver);

		printf("\n");
		printf("kernel present on snapshot:\n");

		for (size_t x = 0; x < kern_list_array[i]->counts; x++)
			printf("\t%s\n", kern_list_array[i]->list[x].kernel_ver);


		free(k->list);
		free(k);

		printf("\n");

	}

out:
	free_system_info(info, SYS_INFO_SHOW);
	for (size_t i = 0; i < info.system.snap.counts; i++) {
		free(subvol_paths[i]);
		free(snap_relative_path[i]);

		free(kern_list_array[i]->list);

		free(kern_list_array[i]);
	}

	free(subvol_paths);
	free(kern_list_array);
	free(snap_relative_path);

	if (fd <! 0) close(fd);
	return status;
}
