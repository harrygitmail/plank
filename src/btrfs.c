#include <btrfsutil.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "btrfs.h"

enum plank_status map_btfs_to_plank(enum btrfs_util_error btrfs_err)
{
	switch (btrfs_err) {
	case BTRFS_UTIL_ERROR_NOT_SUBVOLUME:
		return PLANK_BTRFS_ERR_NOT_SUBVOLUME; break;
	case BTRFS_UTIL_ERROR_NOT_BTRFS:
		return PLANK_BTRFS_ERR_NOT_BTRFS; break;
	}
}

enum plank_status get_snapshot_list(
	int tar_subvol_fd,
	struct snapshot_list *ret)
{
	enum btrfs_util_error B_ret;
	B_ret = btrfs_util_is_subvolume_fd(tar_subvol_fd);

	switch(B_ret) {
	case BTRFS_UTIL_OK: break;
	case BTRFS_UTIL_ERROR_NOT_SUBVOLUME:
		return map_btfs_to_plank(BTRFS_UTIL_ERROR_NOT_SUBVOLUME);
	case BTRFS_UTIL_ERROR_NOT_BTRFS:
		return map_btfs_to_plank(BTRFS_UTIL_ERROR_NOT_BTRFS);
	default: return PLANK_BTRFS_ERR;
	}

	struct btrfs_util_subvolume_info tar_subvol_info;
	B_ret = btrfs_util_subvolume_get_info_fd(tar_subvol_fd, 0, &tar_subvol_info);

	if(B_ret != BTRFS_UTIL_OK) return PLANK_BTRFS_ERR;

	struct btrfs_util_subvolume_iterator *tar_subvol_iter;

	B_ret = btrfs_util_subvolume_iter_create_fd(tar_subvol_fd, 5, 0, &tar_subvol_iter);

	switch(B_ret) {
	case BTRFS_UTIL_OK: break;
	default: return PLANK_BTRFS_ERR;
	}

	char *path;
	struct btrfs_util_subvolume_info subvol_info;

	size_t capacity = 6;
	size_t n = 0;

	ret->list = malloc(sizeof(snapshot_info) * capacity);

	if (ret->list == NULL) return PLANK_MEM_ERR;

	while((B_ret = btrfs_util_subvolume_iter_next_info(
		tar_subvol_iter,
		&path,
		&subvol_info)) == BTRFS_UTIL_OK) {

		int memcmp_ret = memcmp(
			tar_subvol_info.uuid,
			subvol_info.parent_uuid,
			16);
		
		if(memcmp_ret != 0) {
			free(path);
			continue;
		}

		if(capacity - 1 <= n) {

			size_t new_capacity = capacity * 2;

			snapshot_info *temp = realloc(
				ret->list,
				sizeof(snapshot_info) * new_capacity);

			if(temp == NULL) return PLANK_MEM_ERR;

			ret->list = temp;
			capacity = new_capacity;
		}

		ret->list[n].snapshot_id = subvol_info.id;
		ret->list[n].snapshot_time = subvol_info.otime;

		free(path);

		n++;

	}

	btrfs_util_destroy_subvolume_iterator(tar_subvol_iter);

	if (n == 0) return PLANK_BTRFS_NO_SNAPSHOT_FOUND;

	ret->counts = n;

	return PLANK_OK;
}

char *get_subvol_path(uint64_t id, int fd) {
	char *path = NULL;
	enum btrfs_util_error btrfs_err;

	btrfs_err = btrfs_util_subvolume_get_path_fd(fd, id, &path);
	if(btrfs_err != BTRFS_UTIL_OK) return NULL;

	return path;
}

