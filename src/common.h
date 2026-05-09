#pragma once
#include <inttypes.h>
#include <time.h>
#include "btrfs.h"

/* get_entry_token - get entry token from host system.
 * 
 * @ret: pointer to entry token string. 
 *
 */


enum plank_status get_entry_token(char **ret);

/* get_pretty_name - get pretty name feild from os-release filea
 *
 * @ret: pointer to pretty name string. 
 */

enum plank_status get_value_by_key(char **ret, const char *target);

/*get_host_uuid - get uuid of root filesystem
 *
 * @ret: pointer to uuid string.
 *
 * this function have some worse error handlling and return status reporting.
 * eyes needed. 
 */

enum plank_status get_host_uuid(char **ret);

/* get_boot_path - get path to $BOOT
 *
 * @ret: pointer to path string.
 *
 * BUG: if directory  or file named "entries" appear in one of 'path_to_look' 
 * then it may report it as vaild path to $BOOT and will fill string with
 * that path even if that directory may not contain files following implementations described in 
 * bootloader specification. as other bootloader implementation also uses "/loader/entries/"
 * it can be confusing. 
 * one solution could be checking presence of file "entries.srel" and its content
 * as described in UAPI bootloader specification for type #1 entry. or more specifically 
 * partition types of those mount points.
 */

enum plank_status get_boot_path(char **ret);


enum plank_status get_ker_ver_snap_tim(
	char *file_name,
	char *kernel_ver,
	struct timespec *tm);
/*
 * list_files - give list of files at given @path
 * if dirfd is non-zero then path resolution will happen relative to
 * dirfd and it will return list of files.
 */
char **list_files(const char *path, int dirfd, size_t  *const count);

void free_file_list(char **const p, size_t counts);
