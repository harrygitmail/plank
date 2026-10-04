#pragma once
#include "types.h"

void link_loader_entries(
	const struct kernel_list *kern_list,
	const struct snapshot_list *snap_list,
	struct loader_entries *const entries);

struct loader_entries *list_loader_entries(const char *BOOT,
	const char *entry_token);

enum plank_status pre_entrie(struct system_info *s_info,
	struct loader_entry_w **entrie_out);

/**
 * pre_entries:- provide entries needed to be written from system info
 * FIX ME: implementation is not done yet completely.
 *
 * NOTE:- 'pre_entries' assume proivded data by caller is efficent and
 * 	  not corrupted in any mananer.
 */
enum plank_status pre_entries(
	struct system_info *sinfo,
	struct loader_entries_w **entries_w);
