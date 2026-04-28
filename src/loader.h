#pragma once
#include <sys/types.h>
#include <stdio.h>
#include "btrfs.h"
#include "kernel.h"

/* loader_entry - store pointers to memeory that hold specific info
 *
 * plank is in very initial stage so loader_entry_w defination may chnage 
 * in future for batter performace and less headache of managing memory.
 */


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

char **read_conf_file(FILE *file);

/*list_loader_entries - store list of loader entries and its releted info
 * @ilst : pointer to list.
 * @counts : number of entries.
 * @error: indicate any error if there is any
 */

struct loader_entries {
	char **list;
	size_t counts;
	int error;
};

typedef enum {
	ENTRY_OK,
	ENTRY_DELETE_PENDING,
} entry_status;

struct loader_entrie {

	char *file_name;
	struct kernel_ver *kern;
	snapshot_info *snap;
	entry_status status;
	size_t counts;

};

struct loader_entrie *link_loader_entries(
	kernel_list *kern_list,
	struct snapshot_list *snap_list,
	struct loader_entries *entries);

struct loader_entries *list_loader_entries(char *BOOT, char *entry_token);
