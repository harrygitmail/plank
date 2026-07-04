#include "commands.h"
#define _GNU_SOURCE 
#include <stdio.h>
#include <fcntl.h>
#include "common.h"
#include "kernel.h"
#include "loader.h"
#include "plank.h"
#include "parse.h"
#include "mount.h"
#include "types.h"
#include "btrfs.h"
#include "write.h"
#include "blob.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int show_host_info(int argc, char **argv)
{
	struct system_info *info = NULL;
	int status = 0;

	int flags = (SYSINFO_EB | SYSINFO_OS_REL | SYSINFO_SYS);

	info = get_system_info(flags);
	switch (info->status) {
	case PLANK_ERR:
		fprintf(stderr, "something went wrong!!\n");
		status = info->status;
		goto out;

	case PLANK_LIBMOUNT_ERR:
		fprintf(stderr, "libmount operation failed!\n");
		status = info->status;
		goto out;

	case PLANK_LIBBLKID_ERR:
		fprintf(stderr, "libblkid operation failed!\n");
		status = info->status;
		goto out;

	case PLANK_BOOT_NOT_FOUND:
		fprintf(stderr, "Unable to find $BOOT. is it mounted corectly ?\n");
		status = info->status;
		goto out;

	case PLANK_PARM_ERR:
		fprintf(stderr, "permission denied !\n");
		status = info->status;
		goto out;

	case PLANK_BTRFS_NO_SNAPSHOT_FOUND:
		fprintf(stderr, "NO snapshot found\n");
		status = info->status;
		goto show_kern;

	case PLANK_BTRFS_ERR_NOT_BTRFS:
		fprintf(stderr, "root filesystem is not btrfs\n");
		status = info->status;
		goto out;

	case PLANK_BTRFS_ERR:
		fprintf(stderr, "libbtrfsutil operation failed!\n");
		status = info->status;
		goto out;

	case PLANK_OK:
		break;
	}

	printf("following snapshot found\n");

	for (size_t i = 0; i < info->system.snap.counts; i++) {
		printf("\t%" PRIu64 "\n", info->system.snap.list[i].snapshot_id);
	}

	printf("\n");

show_kern:
	if (info->system.kern.counts == 0) {
		fprintf(stdout, "NO kernel found\n\n");
		goto show_next;
	}

	printf("-----------------------------------------------\n");
	printf("following kernel found\n");

	for (size_t i = 0; i < info->system.kern.counts; i++) {
		printf("\t%s\n", info->system.kern.list[i].kernel_ver);
	}

	printf("\n");

show_next:
	printf("----------------------------------------------\n");
	printf("system info:\n\n");

	printf("entry-token	: %s\n", info->system.eb.entry_token);
	printf("$BOOT		: %s\n", info->system.eb.boot_path);
	printf("pretty name	: %s\n", info->os_release.pretty_name);
	printf("ID		: %s\n", info->os_release.id);
	printf("UUID		: %s\n", info->system.mount_info.uuid);

	
out:
	free_system_info(info, flags);
	return status;
}

int make_entrie(int argc, char **argv)
{
	int len;
	int status = 0;

	struct system_info *info = NULL;
	struct loader_entrie_w *host_loader = NULL;

	int flags = (SYSINFO_EB | SYSINFO_SYS | SYSINFO_OS_REL | SYSINFO_LDER);

	info = get_system_info(flags);
	switch (info->status) {
	case PLANK_ERR:
		fprintf(stderr, "something went wrong!!\n");
		status = info->status;
		goto out;

	case PLANK_LIBMOUNT_ERR:
		fprintf(stderr, "libmount operation failed!\n");
		status = info->status;
		goto out;

	case PLANK_LIBBLKID_ERR:
		fprintf(stderr, "libblkid operation failed!\n");
		status = info->status;
		goto out;

	case PLANK_BOOT_NOT_FOUND:
		fprintf(stderr, "Unable to find $BOOT. is it mounted corectly ?\n");
		status = info->status;
		goto out;

	case PLANK_PARM_ERR:
		fprintf(stderr, "permission denied !\n");
		status = info->status;
		goto out;

	case PLANK_BTRFS_NO_SNAPSHOT_FOUND:
		fprintf(stderr, "NO snapshot found\n");
		status = info->status;
		goto out;

	case PLANK_BTRFS_ERR_NOT_BTRFS:
		fprintf(stderr, "root filesystem is not btrfs\n");
		status = info->status;
		goto out;

	case PLANK_BTRFS_ERR:
		fprintf(stderr, "libbtrfsutil operation failed!\n");
		status = info->status;
		goto out;

	case PLANK_OK:
		break;
	}

	int p;
	enum plank_status p_status;

	p_status = pre_entrie(info, &host_loader);

	if (p_status != PLANK_OK) {
		status = p_status;
		goto out;
	}
	
	for(p = 0; ; p++) {
		if(host_loader[p].filename == NULL) break;

		char *path_to_file = NULL;

		len = asprintf(&path_to_file, "%s/loader/entries/%s",
				info->system.eb.boot_path,
				host_loader[p].filename);

		if (len == -1) goto out;

		FILE *file = fopen(path_to_file, "w");

		p_status = write_entrie(&host_loader[p], file);

		if (p_status != PLANK_OK)
			goto out;

		fclose(file);

		free(host_loader[p].filename);
		free(host_loader[p].title);
		free(host_loader[p].version);
		free(host_loader[p].sort_key);
		free(host_loader[p].options);
		free(host_loader[p].kernel);
		free(host_loader[p].initrd);

		free(path_to_file);
	}
out:
	free_system_info(info, flags);
	free(host_loader);

	return status;
}

int clean(int argc , char **argv) {
	enum plank_status ret;
	int tar_subvol_fd;

	struct kernel_list host;
	host.list = NULL;
	struct snapshot_list tar_subvol_snap_info;
	tar_subvol_snap_info.list = NULL;
	struct loader_entries *entries = NULL;

	char *entry_token = NULL;
	char *boot_path = NULL;
	char **list_to_delete = NULL;

	size_t entry_del_count = 0;

	tar_subvol_fd = open("/", O_RDONLY);

	if(tar_subvol_fd == -1) {
		ret = -1;
		goto out;
	}

	ret = get_snap_ls(tar_subvol_fd, &tar_subvol_snap_info);

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

	list_to_delete = malloc(sizeof(char *) * entries->counts);
	if (list_to_delete == NULL) {
		printf("failed to delete entries\n");
		goto out;
	}

	printf("about to delete following files:\n");

	for(size_t i = 0; i < entries->counts; i++) {

	asprintf(&list_to_delete[entry_del_count], "%s/loader/entries/%s",
			boot_path,
			entries->entrie[i].file_name);

	printf("%s\n", list_to_delete[entry_del_count++]);

	}

	printf("hit y and hit enter to delete them\n");
	char input2[100];
	scanf("%s", input2);

	int c2 = strncmp(input2, "y", 1);

	if (c2 != 0) {
		printf("abort!\n");
		goto out;
	}

	for (size_t i = 0; i < entry_del_count; i++)
		unlink(list_to_delete[i]);

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

	if (argc > 1) {
		if (strcmp(argv[1], "-l") == 0)
			goto just_list;

		fprintf(stderr, "use '-l' option to just list entries\n");
		goto exit;
	}
	for (size_t i = 0; i < entries->counts; i++) {

		char *path_to_file = NULL;
		int len = asprintf(&path_to_file,
			"%s/loader/entries/%s",
			path_to_boot,
			entries->entrie[i].file_name);

		if (len == -1) {
			ret = PLANK_MEM_ERR;
			goto exit;
		}

		FILE *file = fopen(path_to_file, "r");
		if (file == NULL) {
			ret = PLANK_ERR;
			goto exit;
		}

		struct loader_entrie_w *entrie = NULL;

		entrie = parse_entrie(file);
		if (entrie == NULL) {
			ret = PLANK_ERR;
			goto exit;
		}

		fclose(file);

		free(path_to_file);

		printf("        title: %s\n", entrie->title);
		printf("      version: %s\n", entrie->version);
		printf("   machine-id: %s\n", entrie->machine_id);
		printf("     sort-key: %s\n", entrie->sort_key);
		printf("      options: %s\n", entrie->options);
		printf("        linux: %s\n", entrie->kernel);
		printf("       initrd: %s\n", entrie->initrd);

		printf("\n");

		free(entrie->title);
		free(entrie->version);
		free(entrie->machine_id);
		free(entrie->sort_key);
		free(entrie->options);
		free(entrie->kernel);
		free(entrie->initrd);

		free(entrie);
	}

	goto exit;

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
	struct system_info *info = NULL;
	int status = 0;
	char **subvol_paths = NULL;
	char **snap_relative_path = NULL;
	struct kernel_list **kern_list_array = NULL;

	int flags = (SYSINFO_EB | SYSINFO_SYS);

	info = get_system_info(flags);
	switch (info->status) {
	case PLANK_ERR:
		fprintf(stderr, "something went wrong!!\n");
		status = info->status;
		goto out;

	case PLANK_LIBMOUNT_ERR:
		fprintf(stderr, "libmount operation failed!\n");
		status = info->status;
		goto out;

	case PLANK_LIBBLKID_ERR:
		fprintf(stderr, "libblkid operation failed!\n");
		status = info->status;
		goto out;

	case PLANK_BOOT_NOT_FOUND:
		fprintf(stderr, "Unable to find $BOOT. is it mounted corectly ?\n");
		status = info->status;
		goto out;

	case PLANK_PARM_ERR:
		fprintf(stderr, "permission denied !\n");
		status = info->status;
		goto out;

	case PLANK_BTRFS_NO_SNAPSHOT_FOUND:
		fprintf(stderr, "NO snapshot found\n");
		status = info->status;
		goto out;

	case PLANK_BTRFS_ERR_NOT_BTRFS:
		fprintf(stderr, "root filesystem is not btrfs\n");
		status = info->status;
		goto out;

	case PLANK_BTRFS_ERR:
		fprintf(stderr, "libbtrfsutil operation failed!\n");
		status = info->status;
		goto out;

	case PLANK_OK:
		break;
	}
	enum plank_status ret ;

	ret = mount_top_subvol(info->system.mount_info, "/mnt");

	if (ret != PLANK_OK) {
		fprintf(stderr, "something went wrong");
		goto out;
	}

	size_t capacity = info->system.snap.counts;

	subvol_paths = malloc(sizeof(char *) * capacity);
	kern_list_array = malloc(sizeof(struct kernel_list *) * capacity);
	snap_relative_path = malloc(sizeof(char *) * capacity);

	int fd = open("/mnt", O_RDONLY);
	if (fd < 0) goto out;

	for (size_t i = 0; i < info->system.snap.counts; i++) {
		kern_list_array[i] = malloc(sizeof(struct kernel_list));

		snap_relative_path[i] = get_subvol_path(
			info->system.snap.list[i].snapshot_id,
			fd);

		asprintf(&subvol_paths[i], "/mnt/%s",
			snap_relative_path[i]);

		get_kernel_list(kern_list_array[i], subvol_paths[i]);

	}

	for (size_t i = 0; i < capacity; i++) {
		printf("snapshot id: %" PRIu64 "\n",
			info->system.snap.list[i].snapshot_id);

		printf("snapshot path: %s\n",
			snap_relative_path[i]);

		struct kernel_list *k = NULL;

		k = kernel_diff(
			&info->system.kern,
			kern_list_array[i],
			KERN_P1);

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

	close(fd);

	ret = umount_top_subvol("/mnt");
	if (ret == PLANK_LIBMOUNT_ERR) {
		fprintf(stderr, "libmount operation failed");
		goto out;
	}
out:
	for (size_t i = 0; i < info->system.snap.counts; i++) {
		free(subvol_paths[i]);
		free(snap_relative_path[i]);

		free(kern_list_array[i]->list);

		free(kern_list_array[i]);
	}

	free_system_info(info, flags);

	free(subvol_paths);
	free(kern_list_array);
	free(snap_relative_path);

	return status;
}

int ls_subvol(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "please provide path to btrfs filesystem\n");
		return -2;
	}

	char *tar_fs = argv[1];

	int btrfs = open(tar_fs, O_RDONLY);
	if (btrfs == -1) {
		perror("open");
		return -1;
	}

	struct subvol_list subvol_ls;
	subvol_ls.subvols = NULL;
	subvol_ls.total = 0;

	struct sub_ref *ref = NULL;
	struct bnode *head = NULL;

	enum plank_status ret = get_subvol_list(&subvol_ls, btrfs);
	switch (ret) {

	case PLANK_BTRFS_ERR_NOT_BTRFS:
		fprintf(stderr, "%s is not btrfs filesystem\n", tar_fs);
		goto out;

	case PLANK_PARM_ERR:
		fprintf(stderr, "permission denied!\n");
		goto out;

	case PLANK_BTRFS_ERR:
		fprintf(stderr, "btrfs operation failed\n");
		goto out;

	case PLANK_OK:
		break;
	}

	if (argc > 2 ) {
		char *op = argv[2];
		if (strcmp(op, "-t") == 0)
			goto show_tree;

		fprintf(stderr,
			"'%s' invaild option use '-t' to get tree view of snapshots\n", op);

		free(subvol_ls.subvols);

		return -3;

	}

	for (size_t i = 0; i < subvol_ls.total; i++) {
		printf("subvol id: 		%" PRIu64 "\n",
			subvol_ls.subvols[i].id);

		printf("subvol parent id:	%" PRIu64 "\n",
			subvol_ls.subvols[i].par_id);

		printf("subvol uuid:		");
		printf_uuid(subvol_ls.subvols[i].uuid);
		printf("\n");

		printf("subvol parent uuid:	");
		printf_uuid(subvol_ls.subvols[i].par_uuid);
		printf("\n");

		char *path = get_subvol_path(subvol_ls.subvols[i].id, btrfs);

		printf("subvol path:		%s\n", path);

		free(path);
		printf("\n");

	}

	goto out;

show_tree:

	ref = int_sub_ref(&subvol_ls);
	if (ref == NULL) {
		ret = -1;
		goto out;
	}

	sbref_up(ref, &subvol_ls);

	head = tree(ref, subvol_ls.total);

	if (head == NULL) {
		ret = -1;
		goto out;
	}

	ptree(head, 0);

out:
	free(subvol_ls.subvols);
	close(btrfs);
	free(ref);

	free_tree(head);

	return ret;
}

int blob(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "%s: please provide command or use --help\n", argv[0]);
		return -1;
	}

	char *cmd = argv[1];
	if (strcmp(cmd, "--help") == 0) {
		blob_usage();
		return 1;
	}

	if (strcmp(cmd, "add") == 0)
		return add(argc - 1, argv + 1);

	if (strcmp(cmd, "show") == 0)
		return show(argc - 1, argv + 1);

	if (strcmp(cmd, "remove") == 0)
		return rm(argc - 1, argv + 1);

	fprintf(stderr, "'%s' unknown command\n", cmd);
	blob_usage();
	return 1;
}
