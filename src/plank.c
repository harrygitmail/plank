#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "btrfs.h"
#include "common.h"
#include "kernel.h"
#include "types.h"
#include "plank.h"
#include "mount.h"

struct system_info get_system_info(int type)
{
	char *entry_token = NULL;
	char *boot_path = NULL;
	char *pretty_name = NULL;
	char *id = NULL;

	int tar_subvol_fd = -1;

	kernel_list kern;
	kern.list = NULL;
	kern.counts = 0;

	struct snapshot_list snap;
	snap.list = NULL;
	snap.counts = 0;

	struct loader_entries entries;
	entries.entrie = NULL;
	entries.counts = 0;
	entries.status = 0;

	struct system_mount_info mount_info;

	struct system_info ret_info;

	enum plank_status ret;

	ret = get_entry_token(&entry_token);
	if (ret == PLANK_ERR) goto out;

	ret_info.system.entry_token = entry_token;

	ret = get_boot_path(&boot_path);
	if (ret == PLANK_BOOT_NOT_FOUND) goto out;

	ret_info.system.boot_path = boot_path;

	switch (type) {
	case SYS_INFO_SHOW:
		ret = get_value_by_key(&pretty_name, "PRETTY_NAME");
		if (ret == PLANK_ERR) goto out;

		ret = get_value_by_key(&id, "ID");
		if (ret == PLANK_ERR) goto out;

		ret = get_kernel_list(&kern, "/");
		if (ret == PLANK_ERR) goto out;

		tar_subvol_fd = open("/", O_RDONLY);
		if (tar_subvol_fd < 0) {
			ret = PLANK_ERR;
			goto out;
		}

		ret = get_snapshot_list(tar_subvol_fd, &snap);
		if (ret == PLANK_ERR) goto out;

		ret = get_mount_info(SYS_MNT_UUID, &mount_info);
		if (ret == PLANK_ERR) {
			ret = PLANK_LIBMOUNT_ERR;
			goto out;
		}

		ret_info.os_release.pretty_name = pretty_name;
		ret_info.os_release.id = id;
		ret_info.system.snap = snap;
		ret_info.system.kern = kern;
		ret_info.system.mount_info = mount_info;

		break;

	case SYS_INFO_WRITE:
		ret = get_value_by_key(&pretty_name, "PRETTY_NAME");
		if (ret == PLANK_ERR) goto out;

		ret = get_value_by_key(&id, "ID");
		if (ret == PLANK_ERR) goto out;

		ret = get_kernel_list(&kern, "/");
		if (ret == PLANK_ERR) goto out;

		tar_subvol_fd = open("/", O_RDONLY);
		if (tar_subvol_fd < 0) {
			ret = PLANK_ERR;
			goto out;
		}

		ret = get_snapshot_list(tar_subvol_fd, &snap);
		if (ret == PLANK_ERR) goto out;

		ret = get_mount_info(SYS_MNT_UUID, &mount_info);
		if (ret == PLANK_ERR) {
			ret = PLANK_LIBMOUNT_ERR;
			goto out;
		}

		ret_info.os_release.pretty_name = pretty_name;
		ret_info.os_release.id = id;
		ret_info.system.snap = snap;
		ret_info.system.kern = kern;
		ret_info.system.mount_info = mount_info;

		break;

	}


out:
	if(tar_subvol_fd <! 0 ) close(tar_subvol_fd);

	ret_info.status = ret;
	return ret_info;
}

void free_system_info(struct system_info info, int type)
{
	free(info.system.entry_token);
	free(info.system.boot_path);

	switch (type) {
	case SYS_INFO_SHOW:
		free(info.os_release.pretty_name);
		free(info.os_release.id);
		free(info.system.kern.list);
		free(info.system.snap.list);
		break;

	case SYS_INFO_WRITE:
		free(info.os_release.pretty_name);
		free(info.os_release.id);
		free(info.system.kern.list);
		free(info.system.snap.list);
		break;

	}
}
