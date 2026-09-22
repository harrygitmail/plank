#define _XOPEN_SOURCE
#define _GNU_SOURCE
#include <stddef.h>
#include <stdint.h>
#include "types.h"
#include "entr.h"
#include "parse.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char *const entr_time_format = "%d-%m-%Y %H:%M:%S";
static int file_name(
	struct kernel_ver *kver,
	struct snapshot_info *snap,
	char *entrie_token,
	char **ret_fl_name);
static int time_to_str(struct timespec time, char **str);
static struct loader_entry *bring_ld_entr(size_t count);
static int grow_cap(struct loader_entries *entries, size_t newcap);
static uint64_t fnv(const void *p, size_t size);

static struct loader_entry *bring_ld_entr(size_t count)
{
	struct loader_entry *ret = NULL;

	ret = calloc(count, sizeof(struct loader_entry));
	if (ret == NULL)
		return NULL;

	return ret;
}

static struct loader_entry_w *bring_ld_entr_w(size_t count)
{
	struct loader_entry_w *ret = NULL;
	ret = calloc(count, sizeof(struct loader_entry_w));
	if (!ret)
		return NULL;

	return ret;
}

static int grow_cap(struct loader_entries *entries, size_t newcap)
{
	if (entries == NULL)
		return -1;

	/**
	 * 'newcap' being 0 is not error. passing it to 'realloc()' may
	 *  free memory block pointed by 'entries->entrie' which is not
	 *  'grow_cap()' suppose to do. caller should use 'free_ld_entrs()'
	 *  to free memory. so just return safetly inserted of reporting it
	 *  as error to caller.
	 */
	if (newcap == 0)
		return 0;

	struct loader_entry *entrie = NULL;

	entrie = realloc(entries->entrie, sizeof(struct loader_entry) * newcap);
	if (entrie == NULL)
		return -1;

	/**
	 * new memory added by 'realloc()' will not be initialized.
	 * so lets initialize new memory area to zero
	 *
	 * we are assuming that 'entries->counts' is number of entries
	 * that already have data and new memory is added after that
	 */

	size_t init_entries = newcap - entries->counts;
	size_t init_mem = sizeof(struct loader_entry) * init_entries;
	size_t last_entrie = entries->counts;

	memset(&entrie[last_entrie], 0, init_mem);

	entries->entrie = entrie;
	entries->cap = newcap;
	return 0;
}

static int grow_cap_w(struct loader_entries_w *entrs_w, size_t newcap)
{
	if (entrs_w == NULL)
		return -1;

	/**
	 * read comments on above function
	 */
	if (newcap == 0)
		return 0;

	struct loader_entry_w *entr_w_ret = NULL;
	entr_w_ret = realloc(
		entrs_w->entrie,
		sizeof(struct loader_entry_w) * newcap);
	if (!entr_w_ret)
		return -1;

	size_t init_entries = newcap - entrs_w->curr;
	size_t init_mem = sizeof(struct loader_entry) * init_entries;
	size_t last_entrie = entrs_w->curr;

	memset(&entr_w_ret[last_entrie], 0, init_mem);

	entrs_w->entrie = entr_w_ret;
	entrs_w->cap = newcap;
	return 0;
}

static uint64_t fnv(const void *in, size_t size)
{
	const unsigned char *p = in;
	uint64_t ret = 0;

	if (in == NULL)
		goto out;

	for (size_t i = 0; i < size; i++)
		ret = (ret << 5) & (ret >> 3) & p[i];
out:
	return ret;
}

struct loader_entries *bring_ld_entrs(size_t entries_count)
{
	struct loader_entries *ret = NULL;
	struct loader_entry *entrie = NULL;

	ret = calloc(1 , sizeof(struct loader_entries));
	if (ret == NULL) {
		perror("bring_ld_entrs(calloc)");
		goto fail;
	}

	entrie = bring_ld_entr(entries_count);
	if (entrie == NULL)
		goto fail;

	ret->cap = entries_count;
	ret->entrie = entrie;
	ret->status = 0;

	return ret;
fail:
	fprintf(stderr,
		"bring_ld_entrs: "
		"failed to allocate loader entries object\n");

	return NULL;
}

struct loader_entries_w *bring_ld_entrs_w(size_t entries)
{
	struct loader_entries_w *ret = NULL;
	struct loader_entry_w *entry = NULL;

	entry = bring_ld_entr_w(entries);
	if (!entry)
		return NULL;

	ret = calloc(1, sizeof(struct loader_entries_w));
	if (!ret)
		goto fail;

	ret->entrie = entry;
	ret->cap = entries;
	ret->curr = 0;
	return ret;
fail:
	fprintf(stderr,
		"bring_ld_entrs_w: failed to allocate loader entries object\n");
	return NULL;
}

void free_ld_entrs(struct loader_entries *entrs)
{
	if (entrs == NULL)
		return;

	for (size_t i = 0; i < entrs->counts; i++) {
		struct loader_entry *entr = &entrs->entrie[i];
		if (entr->free & FREE_FL_NAME)
			free(entr->file_name);
		if (entr->free & FREE_PRETYNA)
			free(entr->pretty_name);
		if (entr->free & FREE_ENTRTOK)
			free(entr->entry_token);
		if (entr->free & FREE_ID)
			free(entr->id);
		if (entr->free & FREE_KERN)
			free(entr->kern);
		if (entr->free & FREE_SNAP)
			free(entr->snap);
		if (entr->free & FREE_SURUUID)
			free(entr->sur_uuid);
	}

	free(entrs->entrie);

	free(entrs);
}

void free_ld_entrs_w(struct loader_entries_w *entrs_w)
{
	if (entrs_w == NULL)
		return;

	for (size_t i = 0; i < entrs_w->curr; i++) {
		free(entrs_w->entrie[i].filename);
		free(entrs_w->entrie[i].title);
		free(entrs_w->entrie[i].version);
		free(entrs_w->entrie[i].machine_id);
		free(entrs_w->entrie[i].sort_key);
		free(entrs_w->entrie[i].options);
		free(entrs_w->entrie[i].kernel);
		free(entrs_w->entrie[i].initrd);
	}

	free(entrs_w->entrie);
	free(entrs_w);
}

static int is_inval_entr_ks(
	struct kernel_list *klist,
	struct snapshot_list *snaplist)
{
	if (klist == NULL || snaplist == NULL)
		goto pass_null;

	return 0;

pass_null:
	fprintf(stderr,
		"entr_mk: "
		"passed NULL pointer\n");
	fprintf(stderr,
		"entr_mk: "
		"invalid argurement passed\n");

	return 1;
}

struct loader_entries *entr_ks(
	struct kernel_list *klist,
	struct snapshot_list *slist)
{
	if (is_inval_entr_ks(klist, slist))
		goto fail;

	struct loader_entries *ret = NULL;
	struct snapshot_info *snap = NULL;
	struct kernel_ver *kern = NULL;

	size_t kern_count = 0;
	size_t snap_count = 0;
	size_t entr_total = 0;
	size_t entr_curre = 0;

	kern_count = klist->counts;
	snap_count = slist->counts;
	entr_total = kern_count * snap_count;

	snap = slist->list;
	kern = klist->list;

	ret = bring_ld_entrs(entr_total);
	if (!ret)
		goto fail;

	for (size_t i = 0; i < snap_count; i++) {
		for (size_t j = 0; j < kern_count; j++) {
			ret->entrie[entr_curre].snap = &snap[i];
			ret->entrie[entr_curre].kern = &kern[j];

			entr_curre++;
		}
	}

	ret->counts = entr_curre;
	return ret;
fail:
	fprintf(stderr,
		"entr_mk: "
		"failed to make entries object\n");
	return NULL;
}

void entr_mark(
	struct loader_entries *entr,
	entry_status status,
	const int *index)
{
	/**
	 * if caller gives NULL then just return
	 * no need to dereference broken pointer
	 */
	if (entr == NULL)
		return;

	if (index == NULL)
		goto mark_all;

	int curr = 0;
	while (!index) {
		int this = index[curr];
		entr->entrie[this].status = status;
		curr++;
	}

mark_all:
	for (int i = 0; i < entr->counts; i++)
		entr->entrie[i].status = status;
}

static char **give_p_to_update(struct loader_entry *entr, int which)
{
	char **target = NULL;
	switch (which) {
	case P_ENTR_TOKEN:
		target = &entr->entry_token;
		break;

	case P_ID:
		target = &entr->id;
		break;

	case P_PRETTY_NAME:
		target = &entr->pretty_name;
		break;

	case P_SUR_UUID:
		target = &entr->sur_uuid;
		break;

	default:
		fprintf(stderr,
			"give_p_to_update: request to update a unknown pointer. %#x\n",
			which);
		return NULL;
	}

	return target;
}

void entr_up_p(
	struct loader_entries *entrs,
	char *p,
	const int *index,
	int which)
{
	/**
	 * FIX ME: that if statment in loop is ugly should remove it
	 * 	   in next commit
	 */
	if (index == NULL)
		goto update_all;

	int curr = 0;
	int this = 0;
	char **target = NULL;
	while (!index) {
		this = index[curr];
		target = give_p_to_update(&entrs->entrie[this], which);
		if (!target)
			return;

		*target = p;
	}
	return;
update_all:

	for (size_t i = 0; i < entrs->counts; i++) {
		target = give_p_to_update(&entrs->entrie[i], which);
		if (!target)
			return;

		*target = p;
	}
	return;
}

int entr_give_file_name(struct loader_entries *entries, char *entrie_token)
{
	int ret = 0;

	for (int i = 0; i < entries->counts; i++) {
		struct loader_entry *entrie = &entries->entrie[i];

		struct kernel_ver *kern = entrie->kern;
		struct snapshot_info *snap = entrie->snap;

		ret = file_name(kern, snap, entrie_token, &entrie->file_name);
		if (ret)
			goto fail;
	}

	return 0;
fail:
	fprintf(stderr,
		"entr_give_file_name: failed to give filename to entrie\n");
	return -1;
}

static int is_inval_ent_fl_nm(
	struct kernel_ver *kver,
	struct snapshot_info *snap,
	char *ent_token,
	char **p)
{
	if (!kver)
		goto yes;
	if (!snap)
		goto yes;
	if (!p)
		goto yes;
	if (!ent_token)
		goto yes;

	return 0;
yes:
	fprintf(stderr,
		"entr_file_name: "
		"invalid argument passed\n");
	return 1;
}

static int time_to_str(struct timespec time, char **str)
{
	if (str == NULL)
		goto fail;

	size_t max = 34;
	size_t len = 0;
	struct tm *tm = NULL;

	tm = localtime(&time.tv_sec);
	if (tm == NULL)
		goto fail;

	if (*str == NULL)
		goto alloc_str;

	len = strlen(*str);
	if (len >= max)
		goto format_str;
alloc_str:
	*str = realloc(*str, max);
	if (*str == NULL)
		goto fail;
format_str:
	strftime(*str, max, entr_time_format, tm);
	return 0;
fail:
	fprintf(stderr,
		"time_to_str: failed to constructe string from time\n");
	return -1;
}

static int str_to_time(struct timespec *time, const char *str)
{
	if (time == NULL || str == NULL)
		goto inval;

	struct tm tm;
	struct timespec str_time;

	char *leftof = strptime(str, entr_time_format, &tm);
	if (!leftof)
		goto strptime_fail;

	if (*leftof != '\0') {
		/**
		 * if 'leftof' do not point to null byte than input
		 * string probably have more charcter than fomrat we
		 * are using (see entr.h) lets just log it and continue
		 */

		fprintf(stdout,
			"str_to_time: "
			"given string contain for charcter than expected\n"
			"str_to_time: "
			"it is not error you can ignore it unless loader "
			"entry have issue\n");
	}

	unsigned char wday[sizeof(tm.tm_wday)];

	memcpy(&wday, &tm.tm_wday, sizeof(tm.tm_wday));
	tm.tm_wday = -1;
	str_time.tv_sec = mktime(&tm);

	if (tm.tm_wday == -1)
		goto fail;

	memcpy(&tm.tm_mday, &wday, sizeof(tm.tm_mday));

	/**
	 * NOTE:- nanosecounds are lost. caller should keep it in mind
	 * 	  string to time conservation may have different time
	 * 	  than actual subvolume creation time. it is recommanded
	 * 	  to use subvolume id to get more accurate time of subvolume
	 * 	  creation or you can use this time still if only 'tv_sec' is
	 * 	  what you mostly care about
	 */
	*time = str_time;
	return 0;

strptime_fail:
	fprintf(stderr,
		"str_to_time: failed to parse all format in given string\n");
	fprintf(stderr, "str_to_time: following string passed: %s\n", str);
	fprintf(stderr,
		"str_to_time: format used by 'plank': %s\n", entr_time_format);
	goto fail;
inval:
	fprintf(stderr, "str_to_time: invalid argument passed (null pointer)\n");
fail:
	fprintf(stderr, "str_to_time: failed to turn string into time\n");
	return -1;
}
/**
 * for now using following filename format.
 * file name format: snapshot-ENTRY_TOKEN-KERN_VER-SNAP_TIME.conf
 *
 */

static int file_name(
	struct kernel_ver *kver,
	struct snapshot_info *snap,
	char *entrie_token,
	char **ret_fl_name)
{
	if (is_inval_ent_fl_nm(kver, snap, entrie_token, ret_fl_name))
		goto fail;

	int cnt = 0;
	char *ret_str = NULL;

	cnt = asprintf(
		&ret_str,
		"snapshot-%s-%s-%ld.conf",
		entrie_token,
		kver->kernel_ver,
		snap->snapshot_time.tv_sec);
	if (cnt < 0) {
		perror("entr_file_name");
		goto fail;
	}

	*ret_fl_name = ret_str;
	return 0;
fail:
	fprintf(stderr,
		"entr_file_name: "
		"failed to create file name\n");

	return -1;
}

int entr_add(
	struct loader_entries *entries,
	struct loader_entry entrie)
{
	int ret = 0;
	if (entries->counts + 1 > entries->cap) {
		ret = grow_cap(entries, entries->cap + 10);
		if (ret < 0)
			return ret;
	}

	entries->entrie[entries->counts] = entrie;
	entries->counts++;
	return ret;
}

int entr_w_add(struct loader_entries_w *entrs_w, struct loader_entry_w entr)
{
	int ret = 0;
	if (entrs_w->curr + 1 > entrs_w->cap) {
		ret = grow_cap_w(entrs_w, entrs_w->cap + 10);
		if (ret)
			return ret;
	}

	entrs_w->entrie[entrs_w->curr] = entr;
	entrs_w->curr++;
	return ret;
}

int entr_to_entr_w(
	struct loader_entry *entr,
	struct loader_entry_w *entr_w)
{
	if (entr == NULL || entr_w == NULL)
		return -1;

	/**
	 * avoid touching entry which is not marked as make pending
	 */

	if (entr->status != ENTRY_MAKE_PENDING)
		return 1;

	char *filename = NULL;
	char *title = NULL;
	char *sort_key = NULL;
	char *entry_token = NULL;
	char *path_to_kern = NULL;
	char *path_to_initd = NULL;
	char *options = NULL;
	char *version = NULL;
	char *date = NULL;

	int len = 0;

	filename = strdup(entr->file_name);
	if (filename == NULL)
		goto fail;

	len = time_to_str(entr->snap->snapshot_time, &date);
	if (len < 0)
		goto fail;

	len = asprintf(
		&title,
		"snapshot %s %s %s",
		entr->pretty_name,
		date,
		entr->kern->kernel_ver);
	if (len < 0)
		goto fail;

	len = asprintf(
		&sort_key,
		"%s-%s",
		entr->id,
		entr->kern->kernel_ver);
	if (len < 0)
		goto fail;

	len = asprintf(
		&path_to_kern,
		"%s/%s/linux",
		entr->entry_token,
		entr->kern->kernel_ver);
	if (len < 0)
		goto fail;

	len = asprintf(
		&path_to_initd,
		"%s/%s/initrd",
		entr->entry_token,
		entr->kern->kernel_ver);
	if (len < 0)
		goto fail;

	len = asprintf(
		&options,
		"root=UUID=%s rw rootflags=subvolid=%" PRIu64
		" loglevel=3 quite systemd.machine_id=%s",
		entr->sur_uuid,
		entr->snap->snapshot_id,
		entr->entry_token);
	if (len < 0)
		goto fail;

	len = asprintf(
		&version,
		"%s",
		entr->kern->kernel_ver);
	if (len < 0)
		goto fail;

	entry_token = strdup(entr->entry_token);
	if (!entry_token)
		goto fail;

	free(date);

	entr_w->filename = filename;
	entr_w->title = title;
	entr_w->sort_key = sort_key;
	entr_w->kernel = path_to_kern;
	entr_w->initrd = path_to_initd;
	entr_w->machine_id = entry_token;
	entr_w->options = options;
	entr_w->version = version;
	return 0;

fail:
	free(filename);
	free(title);
	free(sort_key);
	free(path_to_kern);
	free(path_to_initd);
	free(version);
	fprintf(stderr,
		"entr_to_entr_w: failed to prepare entry for writing\n");
	return -1;
}

int entr_w_to_entr(
	struct loader_entry_w *entr_w,
	struct loader_entry *entr)
{
	if (entr == NULL || entr_w == NULL)
		return -1;

	char *pretty_name = NULL;
	char *entry_token = NULL;
	char *sur_uuid = NULL;
	char *kern_str = NULL;
	char *file_name = NULL;
	char *subvolid = NULL;
	char *subvoltime = NULL;
	char *id = NULL;

	struct kernel_ver *kern = NULL;
	struct snapshot_info *snap = NULL;

	int ret = 0;
	long subvolid_l = 0;

	ret = parse_title(entr_w->title, TITLE_PRETTY_NAME, &pretty_name);
	if (ret)
		goto fail;

	ret = parse_option(entr_w->options, OP_UUID, &sur_uuid);
	if (ret)
		goto fail;

	entry_token = strdup(entr_w->machine_id);
	if (!entry_token)
		goto fail;

	ret = parse_title(entr_w->title, TITLE_KERN_VER, &kern_str);
	if (ret)
		goto fail;

	kern = malloc(sizeof(struct kernel_ver));
	if (!kern)
		goto fail;

	strcpy(kern->kernel_ver, kern_str);

	file_name = strdup(entr_w->filename);
	if (!file_name)
		goto fail;

	ret = parse_option(entr_w->options, OP_SUBVOL_ID, &subvolid);
	if (ret)
		goto fail;

	snap = malloc(sizeof(struct snapshot_info));
	if (!snap)
		goto fail;

	ret = parse_title(entr_w->title, TITLE_SNAP_TIME, &subvoltime);
	if (ret)
		goto fail;

	subvolid_l = strtol(subvolid, NULL, 10);
	snap->snapshot_id = (uint64_t) subvolid_l;

	ret = str_to_time(&snap->snapshot_time, subvoltime);
	if (ret)
		goto fail;

	ret = parse_id_skey(&id, entr_w->sort_key);
	if (ret)
		goto fail;

	entr->file_name = file_name;
	entr->entry_token = entry_token;
	entr->sur_uuid = sur_uuid;
	entr->pretty_name = pretty_name;
	entr->id = id;
	entr->kern = kern;
	entr->snap = snap;
	entr->status = ENTRY_EXIST_BOOT;

	int fr = 0;
	fr = (
		FREE_FL_NAME |
		FREE_ENTRTOK |
		FREE_SURUUID |
		FREE_PRETYNA |
		FREE_ID |
		FREE_SNAP |
		FREE_KERN);
	entr->free = fr;

	free(kern_str);
	free(subvolid);
	free(subvoltime);
	return 0;
fail:
	free(pretty_name);
	free(sur_uuid);
	free(entry_token);
	free(kern);
	free(snap);
	free(kern_str);
	free(file_name);
	free(subvolid);
	free(subvoltime);
	free(id);
	fprintf(stderr,
		"entr_w_to_entr: failed to create object 'loader_entrie'\n");
	return -1;
}

int entr_give_id(struct loader_entry *entr)
{
	uint64_t segment = 0;
	size_t offset = 0;

	segment = fnv(entr->entry_token, sizeof(entr->entry_token));
	memcpy(&entr->entry_id[offset], &segment, sizeof(uint64_t));

	offset += sizeof(uint64_t);

	segment = fnv(entr->id, sizeof(entr->id));
	memcpy(&entr->entry_id[offset], &segment, sizeof(uint64_t));
	offset += sizeof(uint64_t);

	segment = fnv(entr->pretty_name, sizeof(entr->pretty_name));
	memcpy(&entr->entry_id[offset], &segment, sizeof(uint64_t));
	offset += sizeof(uint64_t);

	segment = fnv(entr->sur_uuid, sizeof(entr->sur_uuid));
	memcpy(&entr->entry_id[offset], &segment, sizeof(uint64_t));
	offset += sizeof(uint64_t);

	segment = fnv(entr->kern, sizeof(struct kernel_ver));
	memcpy(&entr->entry_id[offset], &segment, sizeof(uint64_t));
	offset += sizeof(uint64_t);

	segment = fnv(entr->snap, sizeof(struct snapshot_info));
	memcpy(&entr->entry_id[offset], &segment, sizeof(uint64_t));

	return 0;
}

static int check_id(
	unsigned char id1[ENTRY_ID_SIZE],
	unsigned char id2[ENTRY_ID_SIZE],
	size_t offset)
{
	return memcmp(&id1[offset], &id2[offset], sizeof(uint64_t));
}

int entr_is_equal(
	struct loader_entry *entr1,
	struct loader_entry *entr2,
	int what)
{
	if (entr1 == NULL || entr2 == NULL)
		return -1;

	size_t offset = 0;

	if (what & ENTR_ENTRY_TOKEN)
		if (check_id(entr1->entry_id, entr2->entry_id, offset))
			return 0;

	offset += sizeof(uint64_t);
	if (what & ENTR_ID)
		if (check_id(entr1->entry_id, entr2->entry_id, offset))
			return 0;

	offset += sizeof(uint64_t);
	if (what & ENTR_PRETTY_NAME)
		if (check_id(entr1->entry_id, entr2->entry_id, offset))
			return 0;

	offset += sizeof(uint64_t);
	if (what & ENTR_SUR_UUID)
		if (check_id(entr1->entry_id, entr2->entry_id, offset))
			return 0;

	offset += sizeof(uint64_t);
	if (what & ENTR_KERN)
		if (check_id(entr1->entry_id, entr2->entry_id, offset))
			return 0;

	offset += sizeof(uint64_t);
	if (what & ENTR_SNAP)
		if (check_id(entr1->entry_id, entr2->entry_id, offset))
			return 0;

	return 1;
}
