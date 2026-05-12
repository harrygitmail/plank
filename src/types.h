#pragma once
#include <inttypes.h>
#include <stddef.h>
#include <time.h>

enum plank_status {
	PLANK_OK,
	PLANK_NO_SNAPSHOT_FOUND,
	PLANK_BTRFS_UTIL_ERR,
	PLANK_BTRFS_UTIL_ERR_NOT_BTRFS,
	PLANK_BTRFS_UTIL_ERR_NOT_SUBVOLUME,
	PLANK_ERR,
	PLANK_MEM_ERR,
	PLANK_BOOT_NOT_FOUND,
	PLANK_NO_ENTRY,
};

typedef struct {
	uint64_t snapshot_id;
	struct timespec snapshot_time;
} snapshot_info;

struct snapshot_list {
	snapshot_info *list;
	size_t counts;
};

struct kernel_ver {
	char kernel_ver[65];
};

typedef struct {
	struct kernel_ver *list;
	size_t counts;

} kernel_list;

typedef struct {

	char *filename;
	char *title;
	char *version;
	char *machine_id;
	char *sort_key;
	char *options;
	char *kernel;
	char *initrd;

} loader_entry_w;


typedef enum {
	ENTRY_OK,
	ENTRY_DELETE_PENDING,
} entry_status;

struct loader_entrie {

	char *file_name;
	struct kernel_ver *kern;
	snapshot_info *snap;
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
		kernel_list kern;
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
