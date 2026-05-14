#pragma once
#include "types.h"

enum plank_status get_mount_info(
		int type,
		struct system_mount_info *ret);

enum plank_status mount_top_subvol(
	struct system_mount_info mount_info,
	const char *target);
