#include <blkid.h>
#include <fcntl.h>
#include <libmount.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include "mount.h"

enum plank_status get_host_uuid(char **ret)
{
	enum plank_status f_ret;
	struct libmnt_table *tb = mnt_new_table();
	struct libmnt_fs *fs = NULL;
	blkid_probe pr = NULL;

	f_ret = mnt_table_parse_mtab(tb, NULL);

	if(f_ret < 0) goto out;

	const char *target_mountpoint = "/";

	fs = mnt_table_find_target(
		tb, target_mountpoint,
		MNT_ITER_BACKWARD);

	if( fs == NULL) {
		f_ret = PLANK_ERR;
		goto out;
	}

	const char *devname = mnt_fs_get_srcpath(fs);

	if(devname == NULL) {
		f_ret = PLANK_ERR;
		goto out;
	}

	pr = blkid_new_probe_from_filename(devname);

	if(pr == NULL) {
		f_ret = PLANK_ERR;
		goto out;
	}

	int ret_tmp;

	ret_tmp = blkid_do_safeprobe(pr);

	if(ret_tmp != 0) {
		f_ret = PLANK_ERR;
		goto out;
	}

	const char *target_uuid = NULL;

	ret_tmp = blkid_probe_lookup_value(
		pr, "UUID", &target_uuid, NULL);

	if(ret_tmp == -1) {
		f_ret = PLANK_ERR;
		goto out;
	}

	*ret = malloc((strlen(target_uuid) + 1));

	strcpy(*ret, target_uuid);

out:
	mnt_unref_table(tb);
	blkid_free_probe(pr);

	return f_ret;
}
