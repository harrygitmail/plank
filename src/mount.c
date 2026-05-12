#include <blkid.h>
#include <fcntl.h>
#include <libmount.h>
#include <sys/stat.h>
#include <string.h>
#include "mount.h"
#include "types.h"

enum plank_status get_mount_info(
	int type,
	struct system_mount_info *ret)
{
	enum plank_status f_ret = PLANK_OK;
	int ret_tmp;
	struct libmnt_table *tb = mnt_new_table();
	struct libmnt_fs *fs = NULL;
	const char *target_uuid = NULL;
	char *source = NULL;
	const char *tag = "UUID";

	f_ret = mnt_table_parse_mtab(tb, NULL);

	if(f_ret < 0) {
		f_ret = PLANK_ERR;
		goto out;
	}

	const char *target_mountpoint = "/";

	fs = mnt_table_find_target(
		tb, target_mountpoint,
		MNT_ITER_BACKWARD);

	if( fs == NULL) {
		f_ret = PLANK_ERR;
		goto out;
	}

	switch (type) {
	case SYS_MNT_UUID:
		ret_tmp = mnt_fs_get_tag(fs, &tag, &target_uuid);

		if (ret_tmp < 0) {
			f_ret = PLANK_ERR;
			goto out;
		}

		ret->type = SYS_MNT_UUID;
		strcpy(ret->uuid, target_uuid);

		break;

	case SYS_MNT_SOURCE:
		source = (char *) mnt_fs_get_source(fs);
		if (source == NULL) {
			f_ret = PLANK_ERR;
			goto out;
		}

		ret->source = strdup(source);

		break;
	}


out:
	mnt_unref_table(tb);

	return f_ret;
}
