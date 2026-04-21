#include <time.h>
#include <inttypes.h>

/* NOTE:- 
 * snapshot in btrfs filesystem are also subvolume with initial share content 
 * from source subvolume. so snapshot can be mounted as seperate filesystem or
 * can be seen as normal directory in btrfs tree just like subvolume.
 *
 * subvolume ID are persistent and cannot be changed. while we can rename or
 * move subvolume by path. thats why subvolume ID are more portable option 
 * than PATH(which can be changed by user later)
 */

/*
 * struct snapshot_info - store btrfs snapshot info 
 * @snapshot_time: time when subvolume was created
 * @snapshot_id: ID of snapshot. 
 */

typedef struct {
	uint64_t snapshot_id;
	struct timespec snapshot_time;
} snapshot_info;

enum plank_status {
	plank_OK,
	plank_NO_SNAPSHOT_FOUND,
	plank_btrfs_util_err,
	plank_btrfs_util_err_NOT_BTRFS,
	plank_btrfs_util_err_NOT_SUBVOLUME,
	plank_err,
	plank_mem_err,
	plank_boot_not_found,
	plank_NO_ENTRY,
};

/* get_snapshot_list - get list of snapshot_info 
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

enum plank_status get_snapshot_list(int tar_subvol_fd, snapshot_info **ret);
