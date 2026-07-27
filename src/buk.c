#include "types.h"
#include "btrfs.h"
#include "buk.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static ssize_t buk_memb_size(struct bucket *buk);
static struct bucket *bring_buk(int type);
static int up_buk_cap(struct bucket *buk, size_t size);
static int buk_grow_data(struct bucket *buk, size_t size);
static int buk_grow_snap(struct bucket *buk, size_t size);
static int buk_grow_subvol(struct bucket *buk, size_t size);
static int is_buk_full(struct bucket *buk);

static ssize_t buk_memb_size(struct bucket *buk)
{
	if (buk == NULL)
		return -1;

	size_t item_size = 0;
	switch (buk->type) {
	case BUK_TYPE_DATA:
		item_size = sizeof(struct data);
		break;

	case BUK_TYPE_SNAP:
		item_size = sizeof(struct snapshot_info);
		break;

	case BUK_TYPE_SUBV:
		item_size = sizeof(struct subvol_info);
		break;

	default:
		fprintf(stderr,
			"buk_memb_size: unknown bucket type '%#x\n'",
			buk->type);
		return -1;
	}

	return item_size;
}

static struct bucket *bring_buk(int type)
{
	struct bucket *ret = NULL;
	size_t cap = 5;

	ret = calloc(1, sizeof(struct bucket));
	if (ret == NULL)
		return NULL;

	ret->type = type;

	switch (type) {
	case BUK_TYPE_DATA:
		ret->data = calloc(cap, sizeof(struct data));
		if (ret->data == NULL)
			goto fail;

		break;

	case BUK_TYPE_SNAP:
		ret->snapls = calloc(cap, sizeof(struct snapshot_info));
		if (ret->snapls == NULL)
			goto fail;

		break;

	case BUK_TYPE_SUBV:
		ret->subvols = calloc(cap, sizeof(struct subvol_info));
		if (ret->subvols == NULL)
			goto fail;

		break;

	default:
		fprintf(stderr,
			"bring_buk: invalid type '%#x' passed",
			ret->type);
		goto fail;
	}

	ret->cap = cap;
	ret->read = 0;
	ret->write = 0;
	ret->fs_fd = -1; /* indicate value is not yet assigned */

fail:
	empty_buk(ret);
	return NULL;
}

static int up_buk_cap(struct bucket *buk, size_t size)
{
	if (size == 0)
		return 0;

	if (buk == NULL)
		return -1;

	ssize_t memb_size = 0;
	memb_size = buk_memb_size(buk);
	if (memb_size == -1)
		return -1;

	size_t cap = size / memb_size;

	buk->cap = cap;
	return 0;
}

static int buk_grow_data(struct bucket *buk, size_t size)
{
	struct data *d = NULL;
	d = realloc(buk->data, size);
	if (d == NULL)
		return -1;

	buk->data = d;
	return up_buk_cap(buk, size);
}

static int buk_grow_snap(struct bucket *buk, size_t size)
{
	struct snapshot_info *snap = NULL;
	snap = realloc(buk->snapls, size);
	if (snap == NULL)
		return -1;

	buk->snapls = snap;
	return up_buk_cap(buk, size);
}

static int buk_grow_subvol(struct bucket *buk, size_t size)
{
	struct subvol_info *subvols = NULL;
	subvols = realloc(buk->subvols, size);
	if (subvols == NULL)
		return -1;

	buk->subvols = subvols;
	return up_buk_cap(buk, size);
}

static int is_buk_full(struct bucket *buk)
{
	if (buk == NULL)
		return -1;

	if (buk->write == buk->cap)
		return 1;

	return 0;
}

int grow_buk(struct bucket *buk, size_t size)
{
	if (size == 0)
		return 0;

	if (buk == NULL)
		return -1;

	switch (buk->type) {
	case BUK_TYPE_DATA:
		return buk_grow_data(buk, size);

	case BUK_TYPE_SUBV:
		return buk_grow_subvol(buk, size);

	case BUK_TYPE_SNAP:
		return buk_grow_snap(buk, size);

	}

	return -1;
}

void empty_buk(struct bucket *buk)
{
	if (buk == NULL)
		return;

	switch (buk->type) {
	case BUK_TYPE_DATA:
		free(buk->data);
		break;

	case BUK_TYPE_SNAP:
		free(buk->snapls);
		break;

	case BUK_TYPE_SUBV:
		free(buk->subvols);
		break;

	}

	if (buk->fs_fd <! 0)
		close(buk->fs_fd);

	free(buk);
}

struct bucket *op_buk(const char *path, int type)
{
	if (path == NULL)
		return NULL;

	struct bucket *buk = NULL;
	int fd = -1;

	buk = bring_buk(type);
	if (buk == NULL)
		return NULL;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		goto op_fail;

	buk->fs_fd = fd;

	return buk;

op_fail:
	perror("op_buk(open)");
	empty_buk(buk);
	return NULL;
}

int add_to_buk(struct bucket *buk, size_t items, void *data)
{
	if (items == 0)
		return 0;

	if (buk == NULL)
		return -1;

	if (data == NULL)
		return -1;

	void *from = NULL;
	void *to = NULL;
	int ret = -1;
	ssize_t item_size = 0;

	from = data;
	item_size = buk_memb_size(buk);
	if (item_size == -1)
		return -1;

	ret = grow_buk(buk, items * item_size);
	if (ret == -1)
		return -1;

	switch (buk->type) {
	case BUK_TYPE_DATA:
		to = (void *) &buk->data[buk->write];
		break;

	case BUK_TYPE_SNAP:
		to = (void *) &buk->snapls[buk->write];
		break;

	case BUK_TYPE_SUBV:
		to = (void *) &buk->subvols[buk->write];
		break;

	default:
		fprintf(stderr,
			"add_to_buk: unknown bucket type '%#x\n'",
			buk->type);
		return -1;
	}

	memcpy(to, from, items * item_size);
	buk->write = buk->write + items;
	return 0;
}

struct bucket *convt_buk(struct bucket *buk, int type)
{
	if (buk == NULL)
		return NULL;

	struct bucket *buk1 = NULL;

	buk1 = bring_buk(BUK_TYPE_SNAP);
	if (buk1 == NULL)
		return NULL;

	struct snapshot_info *sp_info = NULL;

	sp_info = malloc(sizeof(struct snapshot_info) * buk->write);
	if (sp_info == NULL)
		goto fail;

	for (buk->read = 0; buk->read < buk->write; buk->read++) {
		sp_info[buk->read].snapshot_id = buk->data[buk->read].id;
		struct timespec tm;

		int ret = get_sub_time(
			buk->fs_fd,
			buk->data[buk->read].id,
			&tm);

		if (ret < 0)
			goto fail;

		sp_info[buk->read].snapshot_time = tm;
	}

	buk1->fs_fd = buk->fs_fd;
	buk1->snapls = sp_info;
	return buk1;
fail:
	empty_buk(buk1);
	return NULL;
}
