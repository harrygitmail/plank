#pragma once
#include "types.h"

void link_loader_entries(
	const struct kernel_list *kern_list,
	const struct snapshot_list *snap_list,
	struct loader_entries *const entries);

struct loader_entries *list_loader_entries(const char *BOOT,
	const char *entry_token);

enum plank_status pre_entrie(struct system_info *s_info,
	struct loader_entrie_w **entrie_out);
