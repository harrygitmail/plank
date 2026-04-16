#include "btrfs.h"
#include <btrfsutil.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum plank_status get_snapshot_list(int tar_subvol_fd, snapshot_info **ret)
{
	enum btrfs_util_error B_ret;
	B_ret = btrfs_util_is_subvolume_fd(tar_subvol_fd);

	switch(B_ret) {
	case BTRFS_UTIL_OK: break;
	case BTRFS_UTIL_ERROR_NOT_SUBVOLUME: return plank_btrfs_util_err_NOT_SUBVOLUME;
	case BTRFS_UTIL_ERROR_NOT_BTRFS: return plank_btrfs_util_err_NOT_BTRFS;
	default: return plank_btrfs_util_err;
	}

	struct btrfs_util_subvolume_info tar_subvol_info;
	B_ret = btrfs_util_subvolume_get_info_fd(tar_subvol_fd, 0, &tar_subvol_info);

	if(B_ret != BTRFS_UTIL_OK) return plank_btrfs_util_err;

	struct btrfs_util_subvolume_iterator *tar_subvol_iter;

	B_ret = btrfs_util_subvolume_iter_create_fd(tar_subvol_fd, 5, 0, &tar_subvol_iter);

	switch(B_ret) {
	case BTRFS_UTIL_OK: break;
	default: return plank_btrfs_util_err;
	}

	char *path;
	struct btrfs_util_subvolume_info subvol_info;

	size_t capacity = 6;
	size_t n = 0;

	*ret = malloc(sizeof(snapshot_info) * capacity);

	if (*ret == NULL) return plank_err;

	while((B_ret = btrfs_util_subvolume_iter_next_info(
					tar_subvol_iter, &path, &subvol_info)) == BTRFS_UTIL_OK) {

		int memcmp_ret = memcmp(tar_subvol_info.uuid, subvol_info.parent_uuid, 16);
		
		if(memcmp_ret != 0) {
			free(path);
			continue;
		}

		if(capacity - 1 <= n) {

			size_t new_capacity = capacity * 2;

			snapshot_info *temp = realloc(*ret, sizeof(snapshot_info) * new_capacity);

			if(temp == NULL) return plank_err;

			*ret = temp; 
			capacity = new_capacity;
		}

		(*ret)[n].snapshot_id = subvol_info.id;
		(*ret)[n].snapshot_time = subvol_info.otime;

		(*ret)[n + 1].snapshot_id = 0;

		free(path);

		n++;

	}

	btrfs_util_destroy_subvolume_iterator(tar_subvol_iter);

	if (n == 0) return plank_NO_SNAPSHOT_FOUND;

	return plank_OK;
}

