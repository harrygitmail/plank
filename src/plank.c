#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "btrfs.h"
#include "common.h"
#include "kernel.h"
#include "types.h"
#include "plank.h"
#include "mount.h"
#include "loader.h"

struct system_info *get_system_info(int flags)
{
	enum plank_status ret = PLANK_OK;
	char *entry_token = NULL;
	char *boot_path = NULL;
	char *pretty_name = NULL;
	char *id = NULL;

	int tar_subvol_fd = -1;

	struct kernel_list kern;
	kern.list = NULL;
	kern.counts = 0;

	struct snapshot_list snap;
	snap.list = NULL;
	snap.counts = 0;

	struct loader_entries *entries = NULL;

	struct system_mount_info mount_info;
	mount_info.type = 0;
	mount_info.source = NULL;
	memset(mount_info.uuid, 0, 37);

	struct system_info *ret_info = malloc(sizeof(struct system_info));
	if (ret_info == NULL) {
		ret = PLANK_MEM_ERR;
		goto out;
	}
eb:
	if (flags & SYSINFO_EB) {
		ret = get_entry_token(&entry_token);
		if (ret != PLANK_OK)
			goto out;

		ret = get_boot_path(&boot_path);
		if (ret != PLANK_OK)
			goto out;
	}

	if (flags & SYSINFO_SYS) {
		ret = get_kernel_list(&kern, "/");
		if (ret != PLANK_OK)
			goto out;

		tar_subvol_fd = open("/", O_RDONLY);
		if (tar_subvol_fd == -1) {
			ret = PLANK_ERR;
			goto out;
		}

		ret = get_snap_ls(tar_subvol_fd, &snap);
		if (ret != PLANK_OK)
			goto out;

		ret = get_mount_info(SYS_MNT_UUID, &mount_info);
		if (ret != PLANK_OK)
			goto out;

	}

	if (flags & SYSINFO_OS_REL) {
		ret = get_value_by_key(&id, "ID");
		if (ret != PLANK_OK)
			goto out;

		ret = get_value_by_key(&pretty_name, "PRETTY_NAME");
		if (ret != PLANK_OK)
			goto out;

	}

	if (flags & SYSINFO_LDER) {

		if (boot_path == NULL && entry_token == NULL) {
			flags = 0;
			flags |= SYSINFO_EB;
			goto eb;
		}

		entries = list_loader_entries(boot_path, entry_token);
		if (entries == NULL)
			goto out;

	}
out:
	ret_info->system.eb.entry_token = entry_token;
	ret_info->system.eb.boot_path = boot_path;
	ret_info->system.kern = kern;
	ret_info->system.snap = snap;
	ret_info->system.mount_info = mount_info;

	ret_info->os_release.id = id;
	ret_info->os_release.pretty_name = pretty_name;

	ret_info->loader.entries = entries;

	if (tar_subvol_fd <! 0 )
		close(tar_subvol_fd);

	ret_info->status = ret;
	return ret_info;
}

void free_system_info(struct system_info *info, int flags)
{
	if (info == NULL)
		return;

	if (flags & SYSINFO_EB) {
		free(info->system.eb.entry_token);
		free(info->system.eb.boot_path);
	}

	if (flags & SYSINFO_SYS) {
		free(info->system.kern.list);
		free(info->system.snap.list);

		if (info->system.mount_info.type == SYS_MNT_SOURCE)
			free(info->system.mount_info.source);

	}

	if (flags & SYSINFO_OS_REL) {
		free(info->os_release.pretty_name);
		free(info->os_release.id);
	}

	if (flags & SYSINFO_LDER) {
		if (info->loader.entries == NULL)
			goto free_info;

		for (size_t i = 0; i < info->loader.entries->counts; i++)
			free(info->loader.entries->entrie[i].file_name);

		free(info->loader.entries->entrie);
		free(info->loader.entries);
	}
free_info:
	free(info);

}
