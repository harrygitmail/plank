#pragma once
#include <stdio.h>
#include "types.h"

char **read_file(FILE *file);

void link_loader_entries(
	const kernel_list *kern_list,
	const struct snapshot_list *snap_list,
	struct loader_entries *const entries);

struct loader_entries *list_loader_entries(const char *BOOT,const char *entry_token);
