#pragma once
#include "types.h"

#define SYS_INFO_SHOW		1
#define SYS_INFO_WRITE		2
#define SYS_INFO_CLEAN		3
#define SYS_INFO_READ		4

struct system_info get_system_info(int type);

void free_system_info(struct system_info info, int type);
