#pragma once
#include "types.h"

struct system_info *get_system_info(int flags);

void free_system_info(struct system_info *info, int flags);
