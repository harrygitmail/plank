#include <blkid.h>
#include <fcntl.h>
#include <libmount.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <error.h>
#include "mount.h"
#include "common.h"
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

		if (source == NULL) {
			value = NULL;
			goto out;
		}

		value = blkid_get_tag_value(cache, "UUID", source);
		goto out;
	}

out:
	/**
	 * strace show that openat() failed with EACESS when libblkid execute
	 * blkind_get_tag_value() but i have not found that documented in libblkid yet.
	 * maybe i should do little more research. so for now lets dont depends on
	 * errono just report back to user that function failed
	 */

	/**
	 * FIX ME: research on wather libblkid have error detection like
	 * 	   libmount
	 */
	if (value == NULL)
		fprintf(stderr,
			"resolve_mount_tag: "
			"failed to resolve tag\n");

	blkid_put_cache(cache);
	return value;

}
enum plank_status get_root_mount_info(
	int type,
	struct mount_info *ret)
{
	enum plank_status f_ret = PLANK_OK;

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
	if (f_ret < 0) {
		fprintf(stderr,
			"get_root_mount_info: "
			"mnt_table_parse_mtab() failed with excode: "
			"%d\n",
			f_ret);

		f_ret = PLANK_LIBMOUNT_ERR;
		goto out;
	}

	fs = mnt_table_find_target(
		tb, target_mountpoint,
		MNT_ITER_BACKWARD);
	if (fs == NULL) {
		fprintf(stderr,
			"get_root_mount_info: "
			"mnt_table_find_target() failed\n");
		f_ret = PLANK_LIBMOUNT_ERR;
		goto out;
	}

	switch (type) {
	case MNT_UUID:
		target_uuid = resolve_mount_tag(fs, tag);
		if (target_uuid == NULL) {
			f_ret = PLANK_MEM_ERR;
			goto out;
		}

		strcpy(ret->sur_uuid, target_uuid);
		ret->type = MNT_UUID;
		break;

	case MNT_PATH:
		source = (char *) mnt_fs_get_source(fs);
		if (source == NULL) {
			fprintf(stderr,
				"get_root_mount_info: "
				"mnt_fs_get_source failed\n");
			f_ret = PLANK_ERR;
			goto out;
		}

		ret->source = strdup(source);
		if (ret->source == NULL) {
			f_ret = PLANK_MEM_ERR;
			goto out;
		}

		break;
	}


out:
	if (f_ret != PLANK_OK) {
		fprintf(stderr, "get_root_mount_info: failed to get mount info\n");
	}
	ret->use = 0;
	mnt_unref_table(tb);
	free(target_uuid);

	return f_ret;
}

enum plank_status mount_top_subvol(
	struct mount_info *mount_info,
	char *target)
{
	int err;
	struct libmnt_context *mount_context = NULL;
	char *str_uuid = NULL;

	enum plank_status ret = PLANK_OK;

	const char *mount_options = "ro,subvolid=5";
	mount_info->target = target;
	mount_context = mnt_new_context();

	if (mount_info->type == MNT_UUID) {
		err = asprintf(&str_uuid, "UUID=%s", mount_info->sur_uuid);

		if (err == -1) {
			ret = PLANK_MEM_ERR;
			goto out;
		}

		err = mnt_context_set_source(mount_context, str_uuid);

		if (err < 0) {
			fprintf(stderr,
				"mount_top_subvol: "
				"mnt_context_set_source failed \n");

			ret = PLANK_LIBMOUNT_ERR;
			goto out;
		}

	} else {

		err = mnt_context_set_source(mount_context, mount_info->source);
		if (err < 0) {
			fprintf(stderr,
				"mount_top_subvol: "
				"mnt_context_set_source failed \n");

			ret = PLANK_LIBMOUNT_ERR;
			goto out;
		}

	}

	err = mnt_context_set_target(mount_context, target);
	if (err < 0) {
		fprintf(stderr,
			"mount_top_subvol: "
			"mnt_context_set_target failed\n");

		ret = PLANK_LIBMOUNT_ERR;
		goto out;
	}

	err = mnt_context_set_options(mount_context, mount_options);
	if (err < 0) {
		fprintf(stderr,
			"mount_top_subvol: "
			"mnt_context_set_options failed (code #%x) \n", err);

		ret = PLANK_LIBMOUNT_ERR;
		goto out;
	}

	err = mnt_context_set_fstype(mount_context, "btrfs");
	if (err) {
		fprintf(stderr,
			"mount_top_subvol: "
			"mnt_context_set_fstype() failed\n");
		ret = PLANK_LIBMOUNT_ERR;
		goto out;
	}

	err = mnt_context_prepare_mount(mount_context);
	if (err) {
		fprintf(stderr,
			"mount_top_subvol: "
			"mnt_context_prepare_mount failed\n");

		ret = PLANK_LIBMOUNT_ERR;
		goto out;
	}

	err = mnt_context_do_mount(mount_context);
	if (err)
		fprintf(stderr,
			"mount_top_subvol: "
			"mnt_context_do_mount failed\n");

	mount_info->use = mount_info->use + 1;
	mount_info->target = target;

out:
	if (ret == PLANK_LIBMOUNT_ERR) {
		fprintf(stderr, "mount_top_subvol: libmount operation failed\n");
		fprintf(stderr, "mount_top_subvol: following arg passed\n");
		fprintf(stderr,
			"mount_top_subvol: "
			"target: %s\n",
			mount_info->target);
		if (mount_info->type == MNT_PATH) {
			fprintf(stderr, "mount_top_subvol: "
					"source: %s\n",
					mount_info->source);
		} else {
			fprintf(stderr,
				"mount_top_subvol: "
				"source uuid: ");
			printf_uuid2(stderr, mount_info->sur_uuid);
			fprintf(stderr, "\n");
		}

		fprintf(stderr,
			"mount_top_subvol: "
			"string passed to mnt_context_set_source() :"
			"%s\n",
			str_uuid);
	}

	mnt_free_context(mount_context);
	free(str_uuid);
	return ret;
}

enum plank_status _umount(
	char *target)
{
	struct libmnt_context *mount_context = NULL;
	int tmp_ret;
	enum plank_status ret = PLANK_OK;
	mount_context = mnt_new_context();

	if (mount_context == NULL)
		return PLANK_ERR;

	tmp_ret = mnt_context_set_target(mount_context, target);

	if (tmp_ret < 0) {
		ret = PLANK_LIBMOUNT_ERR;
		goto out;
	}

	tmp_ret = mnt_context_umount(mount_context);

	if (tmp_ret > 0) {
		ret = PLANK_LIBMOUNT_ERR;
		goto out;
	}

out:
	mnt_free_context(mount_context);
	return ret;
}

enum plank_status mnt_no_need_now(struct mount_info *info)
{
	info->use = info->use - 1;

	if (info->use > 0)
		return PLANK_OK;

	return _umount(info->target);
}

void free_mnt_info(struct mount_info info)
{
	if (info.type == MNT_PATH) {
		free(info.source);
	}

	free(info.target);
}
