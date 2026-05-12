#pragma once
#include "types.h"

enum plank_status get_mount_info(
		int type,
		struct system_mount_info *ret);
