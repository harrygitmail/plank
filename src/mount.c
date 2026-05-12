#include <blkid.h>
#include <fcntl.h>
#include <libmount.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include "mount.h"
#include "types.h"

static const char *resolve_mount_tag(struct libmnt_fs *fs,
	const char *tag_name)
{
	const char *value = NULL;
	blkid_cache cache = NULL;

	if (mnt_fs_get_tag(fs, &tag_name, &value) == 0)
		goto out;

	/*
	 * lets use libblkid in case above method fail
	 */

	if (blkid_get_cache(&cache, NULL) == 0 ) {
		const char *source = NULL;
		source = mnt_fs_get_source(fs);

		if (source == NULL) goto out;

		value = blkid_get_tag_value(cache, "UUID", source);

		if (value == NULL) goto out;
	} else {
		goto out;
	}

out:
	blkid_put_cache(cache);
	return value;

}
enum plank_status get_mount_info(
	int type,
	struct system_mount_info *ret)
{
	enum plank_status f_ret = PLANK_OK;
	int ret_tmp;

	struct libmnt_fs *fs = NULL;
	char *source = NULL;
	const char *target_uuid = NULL;
	const char *tag = "UUID";
	const char *target_mountpoint = "/";

	struct libmnt_table *tb = mnt_new_table();

	if (tb == NULL) {
		f_ret = PLANK_MEM_ERR;
		goto out;
	}

	f_ret = mnt_table_parse_mtab(tb, NULL);

	if(f_ret < 0) {
		f_ret = PLANK_ERR;
		goto out;
	}

	fs = mnt_table_find_target(
		tb, target_mountpoint,
		MNT_ITER_BACKWARD);

	if( fs == NULL) {
		f_ret = PLANK_ERR;
		goto out;
	}

	switch (type) {
	case SYS_MNT_UUID:
		target_uuid = resolve_mount_tag(fs, tag);
		if (target_uuid == NULL) {
			f_ret = PLANK_MEM_ERR;
			goto out;
		}

		strcpy(ret->uuid, target_uuid);

		ret->type = SYS_MNT_UUID;

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
	free(target_uuid);

	return f_ret;
}
