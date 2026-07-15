#pragma once
#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
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
	PLANK_PARM_ERR,
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

#define MNT_UUID 	0xee
#define MNT_PATH	0xff

struct mount_info {
	int type;
	size_t use;
	union {
		char sur_uuid[37];
		char *source;
	};
	char *target;
};

#define SYSINFO_EB	(1 << 0)
#define SYSINFO_SYS	(1 << 1)
#define SYSINFO_OS_REL	(1 << 2)
#define SYSINFO_LDER	(1 << 3)

struct system_info {

	struct {

		struct {
			char *entry_token;
			char *boot_path;
		} eb;

		struct kernel_list kern;
		struct snapshot_list snap;
		struct mount_info mount_info;

	} system;

	struct {

		char *pretty_name;
		char *id;

	} os_release;

	struct {

		struct loader_entries *entries;

	} loader;

	enum plank_status status;

};
struct subvol_info {
	uint64_t 	id;
	uint64_t 	par_id;
	uint8_t 	uuid[16];
	uint8_t 	par_uuid[16];
	struct timespec otime;
};

struct subvol_list {
	struct subvol_info *subvols;
	size_t total;
};

struct sub_ref {
	uint64_t id;
	size_t child_count;
	void *uuid;
	void *par_uuid;
	uint64_t source;
};

enum bnode_type {
	BNODE_INIT,
	BNODE_LEAF,
	BNODE_SMBR,
	BNODE_DPBR,
};

/**
 * @id : id of subvolume
 * @smdp_br : point to next branch on same depth
 * @dpdp_br : point to next branch on deeper depth
 * @leaf : point to next leaf within same branch
 */

struct bnode {
	uint64_t id;
	struct bnode *smdp_br;
	struct bnode *dpdp_br;
	struct bnode *leaf;
};

#define MAGIC_SIZE		0x8
#define START_OFFSET		0x100

typedef struct {
	uint8_t magic[MAGIC_SIZE];
	uint64_t version;
} blob_header;

struct index {
	uint64_t start_off;
	uint64_t last_off;
	uint64_t total_data;
};

struct data {
	uint64_t id;
};

#define BUK_TYPE_DATA 	0x0aa
#define BUK_TYPE_SNAP 	0x0ab
#define BUK_TYPE_SUBV 	0x0ac

struct bucket {
	int type;
	union {
		struct data *data;
		struct snapshot_info *snapls;
		struct subvol_info *subvols;
	};
	int fs_fd;
	size_t cap;
	size_t read;
	size_t write;
};

#define FILE_READ	(1 << 1)
#define FILE_WRITE	(1 << 2)
#define FILE_NEW	(1 << 3)

struct pfile {
	FILE *fp;
	const char *nm;
	const char *tmp;
	struct index index;
	struct bucket *pbuk;
	int mode;
};

enum file_pos {
	HEADER,
	INDEX,
	DATA_START,
	DATA_END,
};
