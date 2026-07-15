#pragma once
#include "types.h"

enum plank_status get_root_mount_info(
	int type,
	struct mount_info *ret);

enum plank_status mount_top_subvol(
	struct mount_info *mount_info,
	char *target);

enum plank_status _umount(
	char *taget);

enum plank_status mnt_no_need_now(struct mount_info *info);

void free_mnt_info(struct mount_info info);
