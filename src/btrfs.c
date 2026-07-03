#include <btrfsutil.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <stdio.h>
#include "types.h"
#include "btrfs.h"

enum plank_status map_btfs_to_plank(enum btrfs_util_error btrfs_err)
{
	switch (btrfs_err) {
	case BTRFS_UTIL_ERROR_NOT_SUBVOLUME:
		return PLANK_BTRFS_ERR_NOT_SUBVOLUME;
	case BTRFS_UTIL_ERROR_NOT_BTRFS:
		return PLANK_BTRFS_ERR_NOT_BTRFS;
	case BTRFS_UTIL_ERROR_NO_MEMORY:
		return PLANK_MEM_ERR;
	case BTRFS_UTIL_OK:
		return PLANK_OK;
	default:
		printf("libbtrfsutil: %s\n", btrfs_util_strerror(btrfs_err));
		return PLANK_BTRFS_ERR;
	}
}

enum plank_status get_snap_ls(
	int tar_subvol_fd,
	struct snapshot_list *ret)
{
	enum btrfs_util_error B_ret;
	enum plank_status p_ret = PLANK_OK;

	struct btrfs_util_subvolume_info tar_subvol_info;
	struct btrfs_util_subvolume_iterator *tar_subvol_iter;
	struct btrfs_util_subvolume_info subvol_info;

	char *path = NULL;
	size_t capacity = 6;
	size_t n = 0;

	B_ret = btrfs_util_is_subvolume_fd(tar_subvol_fd);
	if (B_ret != BTRFS_UTIL_OK)
		return map_btfs_to_plank(B_ret);

	errno = 0;
	B_ret = btrfs_util_subvolume_get_info_fd(
		tar_subvol_fd,
		0,
		&tar_subvol_info);

	if (B_ret != BTRFS_UTIL_OK) {
		if (errno == EPERM)
			return PLANK_PARM_ERR;

		return map_btfs_to_plank(B_ret);
	}

	B_ret = btrfs_util_subvolume_iter_create_fd(
		tar_subvol_fd,
		5,
		0,
		&tar_subvol_iter);

	if (B_ret != BTRFS_UTIL_OK) {
		p_ret = map_btfs_to_plank(B_ret);
		goto out;
	}

	ret->list = malloc(sizeof(struct snapshot_info) * capacity);
	if (ret->list == NULL) {
		p_ret = PLANK_MEM_ERR;
		goto out;
	}

	errno = 0;
	while((B_ret = btrfs_util_subvolume_iter_next_info(
		tar_subvol_iter,
		&path,
		&subvol_info)) == BTRFS_UTIL_OK) {

		int memcmp_ret = memcmp(
			tar_subvol_info.uuid,
			subvol_info.parent_uuid,
			16);
		
		if(memcmp_ret != 0) {
			free(path);
			continue;
		}

		if(capacity - 1 <= n) {

			size_t new_capacity = capacity * 2;
			struct snapshot_info *temp = realloc(
				ret->list,
				sizeof(struct snapshot_info) * new_capacity);

			if(temp == NULL) {
				p_ret = PLANK_MEM_ERR;
				goto out;
			}

			ret->list = temp;
			capacity = new_capacity;
		}

		ret->list[n].snapshot_id = subvol_info.id;
		ret->list[n].snapshot_time = subvol_info.otime;

		free(path);

		n++;

	}

	ret->counts = n;

	if (B_ret != BTRFS_UTIL_ERROR_STOP_ITERATION) {
		if (errno == EPERM) {
			p_ret = PLANK_PARM_ERR;
			goto out;
		}

		p_ret =  map_btfs_to_plank(B_ret);
		goto out;
	}

	if (n == 0)
		p_ret = PLANK_BTRFS_NO_SNAPSHOT_FOUND;
out:
	btrfs_util_destroy_subvolume_iterator(tar_subvol_iter);
	return p_ret;
}

char *get_subvol_path(uint64_t id, int fd) {
	char *path = NULL;
	enum btrfs_util_error btrfs_err;

	btrfs_err = btrfs_util_subvolume_get_path_fd(fd, id, &path);
	if(btrfs_err != BTRFS_UTIL_OK) return NULL;

	return path;
}

enum plank_status get_subvol_list(struct subvol_list *ret, int fd)
{
	enum btrfs_util_error b_ret;

	b_ret = btrfs_util_is_subvolume_fd(fd);

	if (b_ret == BTRFS_UTIL_ERROR_NOT_BTRFS)
		return PLANK_BTRFS_ERR_NOT_BTRFS;

	if (b_ret != BTRFS_UTIL_OK)
		return PLANK_BTRFS_ERR;

	struct btrfs_util_subvolume_iterator *subvol_iter = NULL;
	b_ret = btrfs_util_subvolume_iter_create_fd(fd, 5, 0, &subvol_iter);

	if (b_ret != BTRFS_UTIL_OK)
		return PLANK_BTRFS_ERR;

	char *path;
	struct btrfs_util_subvolume_info subvol_info;

	size_t capacity = 6;
	size_t n = 0;

	ret->subvols = malloc(sizeof(struct subvol_info) * capacity);

	if (ret->subvols == NULL)
		return PLANK_MEM_ERR;

	while((b_ret = btrfs_util_subvolume_iter_next_info(
		subvol_iter,
		&path,
		&subvol_info)) == BTRFS_UTIL_OK) {

		if(capacity < n + 1) {

			size_t new_capacity = capacity * 2;

			struct subvol_info *temp = realloc(
				ret->subvols,
				sizeof(struct subvol_info) * new_capacity);

			if(temp == NULL) return PLANK_MEM_ERR;

			ret->subvols = temp;
			capacity = new_capacity;
		}
		ret->subvols[n].id = subvol_info.id;
		ret->subvols[n].par_id = subvol_info.parent_id;
		ret->subvols[n].otime = subvol_info.otime;

		memcpy(ret->subvols[n].uuid, subvol_info.uuid, 16);
		memcpy(ret->subvols[n].par_uuid, subvol_info.parent_uuid, 16);

		free(path);

		n++;

	}

	btrfs_util_destroy_subvolume_iterator(subvol_iter);

	ret->total = n;

	return PLANK_OK;
}

struct sub_ref *int_sub_ref(const struct subvol_list *ls)
{
	struct sub_ref *ref = calloc(ls->total + 1, sizeof(struct sub_ref));
	if (ref == NULL)
		return NULL;

	for (size_t i = 0; i < ls->total; i++) {
		ref[i].id = ls->subvols[i].id;
		ref[i].par_uuid = ls->subvols[i].par_uuid;
		ref[i].uuid = ls->subvols[i].uuid;
		ref[i].child_count = 0;
		ref[i].source = 0;
	}

	return ref;
}

void sbref_up(struct sub_ref *ref, struct subvol_list *ls)
{
	size_t total = ls->total;

	for (size_t i = 0; i < total; i++) {
		for (size_t j = 0; j < total; j++) {
			int c = memcmp(ref[i].uuid, ref[j].par_uuid, 16);
			if (c != 0)
				continue;

			ref[j].source = ref[i].id;
			size_t cnt = ref[i].child_count;
			ref[i].child_count = ++cnt;
		}

	}
}

static struct bnode *add_node(
	uint64_t id,
	struct bnode *tar,
	enum bnode_type type)
{
	struct bnode *node = malloc(sizeof(struct bnode));
	if (node == NULL)
		return NULL;

	node->id = id;
	node->leaf = NULL;
	node->smdp_br = NULL;
	node->dpdp_br = NULL;

	switch (type) {
	case BNODE_INIT:
		node->id = 0;
		break;

	case BNODE_LEAF:
		tar->leaf = node;
		break;

	case BNODE_SMBR:
		tar->smdp_br = node;
		break;

	case BNODE_DPBR:
		tar->dpdp_br = node;
		break;

	default:
		free(node);
		node = NULL;
		break;
	}

	return node;
}


void free_tree(struct bnode *node)
{
    if (node == NULL)
        return;

    free_tree(node->dpdp_br);
    free_tree(node->smdp_br);
    free_tree(node->leaf);

    free(node);
}

static int add_all_lef(
	struct sub_ref *ref,
	size_t total,
	struct bnode *br)
{
	uint64_t src_id = br->id;
	struct bnode *br_head = br;
	struct bnode *lef_tail = br_head;

	int n = 0;

	for (size_t i = 0; i < total; i ++) {
		if (ref[i].child_count != 0)
			continue;

		if (ref[i].source != src_id)
			continue;

		struct bnode *cur = add_node(ref[i].id, lef_tail, BNODE_LEAF);
		if (cur == NULL)
			goto fail;

		lef_tail = cur;
		n++;
	}

	return n;

fail:
	free_tree(br->leaf);
	return -1;

}

static int add_all_br(
	struct sub_ref *ref,
	size_t total,
	struct bnode *br)
{
	uint64_t src_id = br->id;

	struct bnode *br_head = br;
	struct bnode *br_tail = br_head;

	int n = 0;

	enum bnode_type ntype;

	for (size_t i = 0; i < total; i++) {
		if (ref[i].child_count == 0)
			continue;

		if (ref[i].source != src_id)
			continue;

		if (n == 0)
			ntype = BNODE_DPBR;
		else
			ntype = BNODE_SMBR;

		struct bnode *curr = add_node(ref[i].id, br_tail, ntype);
		if (curr == NULL)
			goto fail;

		br_tail = curr;
		n++;
	}

	return n;

fail:
	free_tree(br->dpdp_br);
	return -1;
}

static int mktree(
	struct sub_ref *ref,
	size_t total,
	struct bnode *node)
{
	if (node == NULL)
		return -1;

	int ret = add_all_lef(ref, total, node);
	if (ret == -1)
		return -1;

	ret = add_all_br(ref, total, node);
	if (ret == -1)
		return -1;

	if (node->smdp_br != NULL)
		if (mktree(ref, total, node->smdp_br) == -1)
			return -1;

	if (node->dpdp_br != NULL)
		if (mktree(ref, total, node->dpdp_br) == -1)
			return -1;


	return 0;

}

struct bnode *tree(struct sub_ref *ref, size_t total)
{
	struct bnode *head = add_node(0, NULL, BNODE_INIT);
	if (head == NULL)
		return NULL;

	int ret = mktree(ref, total, head);
	if (ret == -1)
		goto fail;

	return head;

fail:
	free_tree(head);
	return NULL;
}

void ptree(struct bnode *node, int depth)
{
	struct bnode *l;
	if (node == NULL)
		return;

	if (node->id == 0) {
		printf("| <-- root branch\n");
		goto next;
	}

	printf("|");
	for (size_t i = 1; i < depth; i++)
		printf(" |");

	printf("-%" PRIu64 "\n", node->id);

next:
	l = node->leaf;
	while (l != NULL) {
		printf("|");
		for (size_t i = 0; i < depth; i++)
			printf(" |");
		printf("-%" PRIu64 "\n", l->id);

		l = l->leaf;
	}

	ptree(node->dpdp_br, depth + 1);

	ptree(node->smdp_br, depth);


}
