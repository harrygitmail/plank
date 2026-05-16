#pragma once
#include <inttypes.h>
#include <stddef.h>
#include <time.h>

enum plank_status {
	PLANK_OK,
	PLANK_BTRFS_NO_SNAPSHOT_FOUND,
	PLANK_BTRFS_ERR,
	PLANK_BTRFS_ERR_NOT_BTRFS,
	PLANK_BTRFS_ERR_NOT_SUBVOLUME,
	PLANK_ERR,
	PLANK_MEM_ERR,
	PLANK_LIBMOUNT_ERR,
	PLANK_LIBBLKID_ERR,
	PLANK_BOOT_NOT_FOUND,
	PLANK_NO_ENTRY,
};

struct snapshot_info{
	uint64_t snapshot_id;
	struct timespec snapshot_time;
};

struct snapshot_list {
	struct snapshot_info *list;
	size_t counts;
};

struct kernel_ver {
	char kernel_ver[65];
};

struct kernel_list{
	struct kernel_ver *list;
	size_t counts;

};

struct loader_entrie_w{

	char *filename;
	char *title;
	char *version;
	char *machine_id;
	char *sort_key;
	char *options;
	char *kernel;
	char *initrd;

};


typedef enum {
	ENTRY_OK,
	ENTRY_DELETE_PENDING,
} entry_status;

struct loader_entrie {

	char *file_name;
	struct kernel_ver *kern;
	struct snapshot_info *snap;
	entry_status status;

};

struct loader_entries {
	struct loader_entrie *entrie;
	size_t counts;
	int status;
};

#define SYS_MNT_UUID 	1
#define SYS_MNT_SOURCE	2

struct system_mount_info {
	int type;
	union {
		char uuid[37];
		char *source;
	};
};

struct system_info {

	struct {

		char *entry_token;
		char *boot_path;
		struct kernel_list kern;
		struct snapshot_list snap;
		struct system_mount_info mount_info;

	} system;

	struct {

		char *pretty_name;
		char *id;

	} os_release;

	struct {

		struct loader_entries entries;

	} loader;

	enum plank_status status;

};
