#include "btrfs.h"
#include "kernel.h"
#include "loader.h"

struct system_info {

	struct {

		char *entry_token;
		char *boot_path;
		kernel_list kern;
		struct snapshot_list snap;
		char uuid[37];

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

#define SYS_INFO_SHOW		1
#define SYS_INFO_WRITE		2
#define SYS_INFO_CLEAN		3
#define SYS_INFO_READ		4

struct system_info get_system_info(int type);

void free_system_info(struct system_info info, int type);
