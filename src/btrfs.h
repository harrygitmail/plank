#pragma once
#include "types.h"

/* NOTE:- 
 * snapshot in btrfs filesystem are also subvolume with initial share content 
 * from source subvolume. so snapshot can be mounted as seperate filesystem or
 * can be seen as normal directory in btrfs tree just like subvolume.
 *
 * subvolume ID are persistent and cannot be changed. while we can rename or
 * move subvolume by path. thats why subvolume ID are more portable option 
 * than PATH(which can be changed by user later)
 */

/* *
 * get_snapshot_list - get list of snapshot_info
 * @tar_subvol_fd: fd of opened path that point to target subvolume.
 * @ret: pointer to snapshot_info.
 *
 * Return: return enum plank_status. on success return plank_OK, on failure due
 * to libbtrfsutil calls return plank_btrfs_util_err, when no snapshot found in
 * filesystem it return plank_NO_SNAPSHOT_FOUND
 *
 * it is recommened to free array after use or on failure of call to avoid 
 * memory leaks.
 */

enum plank_status get_snapshot_list(
	int tar_subvol_fd, 
	struct snapshot_list *ret);

char *get_subvol_path(uint64_t id, int fd);

enum plank_status get_subvol_list(
	struct subvol_list *ret,
	int fd);

struct sub_ref *int_sub_ref(const struct subvol_list *ls);

void sbref_up(struct sub_ref *ref, struct subvol_list *ls);


void free_tree(struct bnode *node);

struct bnode *tree(struct sub_ref *ref, size_t total);

void ptree(struct bnode *node, int depth);
