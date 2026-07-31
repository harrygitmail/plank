#include "common.h"
#include "mount.h"
#include "types.h"
#include "blob.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>

static int set_at(struct pfile *file, enum file_pos pos);

#define MAX_ZERO_WRITE 0x50

static int write_zero(struct pfile *file, uint64_t offset, size_t size);
static int write_ix(struct pfile *file);
static int read_ix(struct pfile *file);

static int write_header(struct pfile *file);
static int check_header(struct pfile *file);

static int write_data(struct bucket *buk, struct pfile *file);
static int read_data(struct bucket **buk, struct pfile *file);

static struct bucket *bring_bucket();
static struct bucket *do_bucket(
	struct bucket *buk1,
	struct bucket *buk2,
	int what);

#define BUK_MERGE	0x0a 	/* present in both list onces*/
#define BUK_SUB		0x0b	/* remove in first list from present in secound */

static struct bucket *dup_buk(struct bucket *buk);

static int check_bucket(struct bucket *buk);
static int grow_bucket(struct bucket *buk, size_t size);
static int add_to_bucket(struct data d, struct bucket *buk);
static void empty_bucket(struct bucket *buk);
static void show_buk(struct bucket *buk);

static const char *file_mode(int mode);
static void show_pfile(struct pfile *file);

static struct pfile *old_write(const char *nm);
static struct pfile *just_read(const char *nm);
static struct pfile *new_file(const char *nm);
static struct pfile *op_pfile(const char *nm, int mode);
static int cls_pfile(struct pfile *file);

static ssize_t parse_argv(char **argv, int invalcount, struct bucket *buk);
static const char *get_tmp_name(const char *path);

const blob_header pheader = {
	.magic = "PLA_~di",
	.version = 11,
};

static void show_usage();
static void add_usage();
static void rm_usage();

/**
 * while right now 'struct data' include just id so look_up_table wont help
 * much but when we have more data and members in 'struct data' then walking
 * through ids will be batter than walking through data itself.
 *
 * we can remove it if we really not goind to extend 'struct data'
 */

struct look_up_table {
	uint64_t *ids;
	struct data *data;
	size_t table_cur;
	size_t table_cap;
};

static struct look_up_table *bring_table();
static int populate_table(struct bucket *buk, struct look_up_table *tab);
static struct data *look_up(struct look_up_table *tab, uint64_t id);
static ssize_t get_slot(struct look_up_table *tab, uint64_t id);
static int grow_table(struct look_up_table *tab, size_t size);
static void break_table(struct look_up_table *tab);

#define IG_HEADER	(1 << 0)	/* ignore unknown header and continue operation */
#define FLG_VERBOSE	(1 << 1)	/* show more info when function fail */

static int blob_cmd_flags = 0;

typedef struct {
	const char *short_name;
	const char *long_name;
	int flag;
} options;

options option[] = {
	{"-ih", "--ingore-header", IG_HEADER},
	{"-v", "--verbose", FLG_VERBOSE},
	{NULL, NULL, 0},
};

static int is_flag(int flag);
static int enable_flag(char **argv);
static int have_flag_prefix(char *p);

static enum plank_status get_blob_path(char **p);
static enum plank_status done_with_path();

static int set_at(struct pfile *file, enum file_pos pos)
{
	if (file == NULL)
		return -1;

	int ret = fseek(file->fp, 0, SEEK_SET);
	if (ret == -1)
		goto fail;

	if (pos == HEADER)
		return 0;

	ret = fseek(file->fp, sizeof(blob_header), SEEK_CUR);
	if (ret == -1)
		goto fail;

	if (pos == INDEX)
		return 0;

	if (pos == DATA_START) {
		ret = fseek(file->fp, file->index.start_off, SEEK_SET);
		if (ret == -1)
			goto fail;

		return 0;
	}

	if (pos == DATA_END) {
		ret = fseek(file->fp, file->index.last_off, SEEK_SET);
		if (ret == -1)
			goto fail;

		return 0;
	}

	fprintf(stderr, "set_at: invaild input\n");
	return -1;

fail:
	perror("set_at(fseek)");
	fprintf(stderr, "failed to set file pointer\n");
	return -1;
}

static int write_zero(struct pfile *file, uint64_t offset, size_t size)
{
	char zero[MAX_ZERO_WRITE] = {0};
	int ret = 0;
	size_t write = 0;
	size_t cur_wr = 0;
	size_t need_to_write = 0;

	ret = fseek(file->fp, offset, SEEK_SET);
	if (ret == -1)
		goto fseek_fail;
write:
	need_to_write = size - write;

	if (need_to_write > MAX_ZERO_WRITE) {
		cur_wr = fwrite(zero, 1, MAX_ZERO_WRITE, file->fp);
		if (cur_wr < MAX_ZERO_WRITE)
			goto fwrite_fail;

		write = write + MAX_ZERO_WRITE;
	} else {
		cur_wr = fwrite(zero, 1, need_to_write, file->fp);
		if (cur_wr < need_to_write)
			goto fwrite_fail;

		write = write + need_to_write;
	}

	if (size != write)
		goto write;

	if (is_flag(FLG_VERBOSE)) {
		fprintf(stderr, "write_zero: passed offset: %#lx\n", offset);
		fprintf(stderr, "write_zero: passed size: %zu\n", size);
		fprintf(stderr, "write_zero: about to show file info\n");
		show_pfile(file);
	}

	return 0;

fseek_fail:
	perror("write_zero(fseek)");
	goto fail;

fwrite_fail:
	perror("write_zero(fwrite)");
	goto fail;
fail:
	fprintf(stderr, "write_zero: failed to write zeros to file\n");
	if (is_flag(FLG_VERBOSE)) {
		fprintf(stderr, "write_zero: passed offset: %#lx\n", offset);
		fprintf(stderr, "write_zero: passed size: %zu\n", size);
		fprintf(stderr, "write_zero: about to show file info\n");
		show_pfile(file);
	}

	return -1;
}

static int write_ix(struct pfile *file)
{
	if (file == NULL)
		return -1;

	int ret = set_at(file, INDEX);
	if (ret == -1)
		goto fail;

	ret = fwrite(&file->index, sizeof(struct index), 1, file->fp);
	if (ret < 1)
		goto fwrite_fail;

	return 0;

fwrite_fail:

	perror("write_ix(fwrite)");
	goto fail;

fail:
	fprintf(stderr, "write_ix: failed to write intex\n");
	return -1;
}

static int read_ix(struct pfile *file)
{
	int ret = set_at(file, INDEX);
	if (ret == -1)
		goto fail;

	ret = fread(&file->index, sizeof(struct index), 1, file->fp);
	if (ret < 1)
		goto check_file_p;

	return 0;

check_file_p:

	if (feof(file->fp))
		goto eof;

	if (ferror(file->fp))
		goto fread_error;

eof:
	fprintf(stderr, "read_ix: end of file is reached(feof).\n");
	goto fail;

fread_error:

	perror("read_ix(fread)");
	goto fail;

fail:
	fprintf(stderr, "read_ix: failed to read index\n");
	return -1;
}

static int write_header(struct pfile *file)
{
	int ret = set_at(file, HEADER);
	if (ret == -1)
		goto fail;

	size_t write = fwrite(&pheader, sizeof(blob_header), 1, file->fp);
	if (write < 1)
		goto fail;

	return 0;

fail:
	perror("write_header(fwrite)");
	fprintf(stderr, "write_header: writing header failed\n");
	return -1;
}

static int check_header(struct pfile *file)
{
	int ret = set_at(file, HEADER);
	if (ret == -1)
		goto fail;

	blob_header header;
	size_t read = fread(&header, sizeof(blob_header), 1, file->fp);
	if (read < 1)
		goto check;

	if (memcmp(&header, &pheader, sizeof(blob_header)) == 0)
		return 1;

	return 0;

check:
	if (feof(file->fp))
		goto eof_reach;

	if (ferror(file->fp))
		goto read_error;

eof_reach:
	fprintf(stderr, "check_header: end of file is reached."
			" file appear shorter than header size\n");
	goto fail;

read_error:
	perror("check_header");
	goto fail;

fail:
	fprintf(stderr, "check_header: failed to check header\n");
	return -1;
}

static int write_data(struct bucket *buk, struct pfile *file)
{
	int ret = 0;
	size_t write = 0;
	size_t zero_size = 0;
	size_t zero_count = 0;
	uint64_t data_size = 0;
	uint64_t last_data_off = 0;

	ret = set_at(file, DATA_START);
	if (ret == -1)
		goto fail;

	write = fwrite(buk->data, sizeof(struct data), buk->write, file->fp);
	if (write < buk->write)
		goto fwrite_fail;

	data_size = buk->write * sizeof(struct data);

	if (buk->write < file->index.total_data) {
		zero_count = file->index.total_data - buk->write;
		zero_size = zero_count * sizeof(struct data);

		last_data_off = file->index.start_off + data_size;
		ret = write_zero(file, last_data_off, zero_size);
		if (ret == -1)
			goto fail;

		printf("write_data: zero size: %zu\n", zero_size);
		printf("write_data: last data offset : %#lx\n", last_data_off);
		printf("write_data: data size: %zu \n", data_size);
	}

	file->index.total_data = buk->write;
	file->index.last_off = file->index.start_off + data_size;

	return 0;

fwrite_fail:
	perror("write_data(fwrite)");
	goto fail;

fail:
	fprintf(stderr, "write_data: failed to write data\n");
	if (is_flag(FLG_VERBOSE)) {
		fprintf(stderr, "write_data: following file passed\n");
		show_pfile(file);
		fprintf(stderr, "\n");
		fprintf(stderr, "write_data: following data requested to write\n");
		show_buk(buk);
		fprintf(stderr, "\n");
	}
	return -1;
}

static int read_data(struct bucket **buk, struct pfile *file)
{
	if (buk == NULL)
		goto inval_in;

	if (*buk == NULL) {
		*buk = bring_bucket();
		if (*buk == NULL)
			return -1;

	}

	int ret = grow_bucket(*buk, file->index.total_data);
	if (ret == -1)
		goto fail;

	(*buk)->type = BUK_TYPE_DATA;

	ret = set_at(file, DATA_START);
	if (ret == -1)
		goto fail;

	size_t read = fread(
		(*buk)->data,
		sizeof(struct data),
		file->index.total_data,
		file->fp);

	if (read == file->index.total_data)
		goto read_complete;

	if (feof(file->fp))
		goto eof_reach;

	if (ferror(file->fp))
		goto fread_error;

read_complete:

	(*buk)->write = file->index.total_data;
	return 0;

eof_reach:
	if(!is_flag(FLG_VERBOSE))
		goto fail;

	fprintf(stderr,
		"read_data: end of file is reached."
		"file appear shorter than expected\n");

	fprintf(stderr,
		"read_data: data read 		: %zu\n",
		read);

	fprintf(stderr,
		"read_data: data expected 	: %zu\n",
		file->index.total_data);

	fprintf(stderr, "read_data: showing file info\n");
	show_pfile(file);
	fprintf(stderr,
		"read_data: if your debuging program please check file with hexdump\n");
	goto fail;

fread_error:

	perror("read_data(fread)");
	goto fail;

fail:
	fprintf(stderr, "read_data: failed to read complete data\n");
	empty_bucket(*buk);
	*buk = NULL;
	return -1;

inval_in:
	if (is_flag(FLG_VERBOSE)) {
		fprintf(stderr, "read_data: passed NULL pointer\n");
		fprintf(stderr, "read_data: passed invalid input\n");
	}

	return -1;
}

static struct bucket *bring_bucket()
{
	struct bucket *buk = NULL;
	buk = malloc(sizeof(struct bucket));
	if (buk == NULL)
		return NULL;

	buk->cap = 5;
	buk->read = 0;
	buk->write = 0;
	buk->data = NULL;

	struct data *d = malloc(sizeof(struct data) * buk->cap);
	if (d == NULL) {
		free(buk);
		return NULL;
	}

	buk->data = d;

	return buk;
}

static struct bucket *do_bucket(
	struct bucket *buk1,
	struct bucket *buk2,
	int what)
{
	struct look_up_table *tab = NULL;
	struct bucket *buk = NULL;
	struct bucket *dup_buk1 = NULL;

	if (buk1 == NULL)
		goto invalin;

	if (buk2 == NULL)
		goto invalin;

	tab = bring_table();

	int ret = populate_table(buk1, tab);
	if (ret == -1)
		goto fail;

	buk = bring_bucket();
	if (buk == NULL)
		goto fail;

	/**
	 * lets dont overwrite caller data
	 */

	dup_buk1 = dup_buk(buk1);
	if (dup_buk1 == NULL)
		goto fail;

	if (what == BUK_SUB)
		goto sub;

	if (what == BUK_MERGE)
		goto merge;

invalin:
	fprintf(stderr, "do_bucket: invalid input\n");
	goto fail;

merge:
	for (buk1->read = 0; buk1->read < buk1->write; buk1->read++) {
		struct data data;
		memcpy(&data, &buk1->data[buk1->read], sizeof(struct data));

		ret = add_to_bucket(data, buk);
		if (ret == -1)
			goto fail;
	}

	for (buk2->read = 0; buk2->read < buk2->write; buk2->read++) {
		struct data *d = look_up(tab, buk2->data[buk2->read].id);
		if (d != NULL)
			continue;

		struct data data;
		memcpy(&data, &buk2->data[buk2->read], sizeof(struct data));

		ret = add_to_bucket(data, buk);
		if (ret == -1)
			goto fail;
	}

	goto out;
sub:
	for (buk2->read = 0; buk2->read < buk2->write; buk2->read++) {
		ssize_t slot = get_slot(tab, buk2->data[buk2->read].id);

		if (slot == -1)
			continue;

		dup_buk1->data[slot].id = 0;
	}

	for (dup_buk1->read = 0;
		dup_buk1->read < dup_buk1->write;
		dup_buk1->read++) {

		if(dup_buk1->data[dup_buk1->read].id == 0)
			continue;

		ret = add_to_bucket(dup_buk1->data[dup_buk1->read], buk);
		if (ret == -1)
			goto fail;
	}

out:
	empty_bucket(dup_buk1);
	break_table(tab);
	return buk;
fail:
	fprintf(stderr, "do_bucket: failed to do operation on buckets\n");
	if (is_flag(FLG_VERBOSE)) {
		show_buk(buk1);
		show_buk(buk2);
	}

	break_table(tab);
	empty_bucket(buk);
	empty_bucket(dup_buk1);
	return NULL;
}

static struct bucket *dup_buk(struct bucket *buk)
{
	struct bucket *dup = bring_bucket();
	if (dup == NULL)
		return NULL;

	int ret = grow_bucket(dup, buk->cap);
	if (ret == -1)
		goto fail;

	for (buk->read = 0; buk->read < buk->write; buk->read++) {
		ret = add_to_bucket(buk->data[buk->read], dup);
		if (ret == -1)
			goto fail;

	}

	return dup;
fail:
	empty_bucket(dup);
	return NULL;
}

static int check_bucket(struct bucket *buk)
{
	if (buk->cap > buk->write)
		return 0;

	size_t newcap = buk->cap * 2;

	buk->data = realloc(buk->data, sizeof(struct data) * newcap);
	if (buk->data == NULL)
		return -1;

	buk->cap = newcap;
	return 0;
}

static int grow_bucket(struct bucket *buk, size_t size)
{
	if (buk == NULL)
		goto inval_in;
	/**
	 * passing size 0 to realloc()
	 * free allocated memory
	 * its batter to return when 0
	 * is passed because caller may not
	 * want to free memory actually
	 *
	 * caller suppose to use empty_bucket()
	 * to free data and bucket.
	 */

	if (size == 0)
		return 0;

	buk->data = realloc(buk->data, size * sizeof(struct data));
	if (buk->data == NULL)
		goto fail;

	buk->cap = buk->cap + size;
	return 0;

fail:
	if (is_flag(FLG_VERBOSE)) {
		fprintf(stderr, "grow_bucket: failed to grow bucket\n");
		fprintf(stderr, "grow bucket: requseted size : %zu\n", size);
		fprintf(stderr, "grow_bucket: following bucket were passed\n");
		show_buk(buk);
	}

	return -1;

inval_in:
	if (is_flag(FLG_VERBOSE)) {
		fprintf(stderr, "grow_bucket: passed NULL pointer\n");
		fprintf(stderr, "grow_bucket: invalid input\n");
	}

	return -1;
}

static int add_to_bucket(struct data d, struct bucket *buk)
{
	int ret = check_bucket(buk);
	if (ret == -1)
		return -1;

	buk->data[buk->write].id = d.id;
	buk->write++;
	return 0;
}

static void empty_bucket(struct bucket *buk)
{
	if (buk == NULL)
		return;

	free(buk->data);
	free(buk);
}

static void show_buk(struct bucket *buk)
{
	if (buk == NULL)
		return;

	printf("show_buk: bucket capacity : %zu\n", buk->cap);
	printf("show_buk: bucket write	: %zu\n", buk->write);
	printf("show_buk: bucket read	: %zu\n", buk->read);

	if (buk->write == 0) {
		printf("show_buk: bucket is empty\n");
		return;
	}
	printf("show_buk: about to show data stored in bucket\n");

	for (buk->read = 0; buk->read < buk->write; buk->read++)
		printf("data : id = %" PRIu64 "\n", buk->data[buk->read].id);

	return;
}

static const char *file_mode(int mode)
{
	static char buf[64];
	buf[0] = '\0';

	if (mode & FILE_READ)
		strcat(buf, "FILE_READ ");

	if (mode & FILE_WRITE)
		strcat(buf, "FILE_WRITE ");

	if (mode & FILE_NEW)
		strcat(buf, "FILE_NEW ");

	return buf;
}

static void show_pfile(struct pfile *file)
{
	fprintf(stderr,
		"file name		: %s\n",
		file->nm);

	fprintf(stderr,
		"file tmp name		: %s\n",
		file->tmp);

	fprintf(stderr,
		"data start offset	: %#lx\n",
		file->index.start_off);

	fprintf(stderr,
		"data end offset        : %#lx\n",
		file->index.last_off);

	fprintf(stderr,
		"total data entries	: %" PRIu64 "\n",
		file->index.total_data);

	fprintf(stderr,
		"file mode 		: %s\n",
		file_mode(file->mode));

	show_buk(file->pbuk);
}

static struct pfile *new_file(
	const char *nm)
{
	struct pfile *file = calloc(1, sizeof(struct pfile));
	if (file == NULL)
		return NULL;

	char *tmp_name = NULL;
	tmp_name = get_tmp_name(nm);
	if (tmp_name == NULL)
		goto fail;

	FILE *f = fopen(tmp_name, "wb");
	if (f == NULL)
		goto fail;

	struct index ix;
	ix.start_off = START_OFFSET;
	ix.last_off = ix.start_off;
	ix.total_data = 0;

	file->fp = f;
	file->index = ix;
	file->nm = nm;
	file->tmp = tmp_name;
	file->mode |= (FILE_NEW | FILE_WRITE);

	return file;
fail:

	perror("new_file(fopen)");
	free(file);
	free(tmp_name);
	return NULL;
}

static struct pfile *old_write(const char *nm)
{
	struct pfile *file = calloc(1, sizeof(struct pfile));
	if (file == NULL)
		return NULL;

	const char *tmp = get_tmp_name(nm);
	if (tmp == NULL)
		goto fail;

	FILE *f = fopen(tmp, "r+");
	if (f == NULL)
		goto fopen_fail;

	file->fp = f;

	int ret = 0;

	if(check_header(file))
		goto read_ix;

	if (is_flag(IG_HEADER)) {
		fprintf(stderr,
			"old_write: unknown header but -ih or -ignore-header option used\n"
			"	    so ignoring header and continue operation\n\n");

		goto read_ix;
	}

	goto unknown_file;

read_ix:
	ret = read_ix(file);
	if (ret == -1)
		goto fail;

	ret = read_data(&file->pbuk, file);
	if (ret == -1)
		goto fail;

	ret = fseek(f, 0, SEEK_CUR);
	if (ret == -1)
		goto fseek_fail;

	file->fp = f;
	file->nm = nm;
	file->tmp = tmp;
	file->mode |= FILE_WRITE;

	return file;

fseek_fail:
	perror("old_write(fseek)");
	goto fail;

fopen_fail:

	perror("old_write(fopen)");
	goto fail;

unknown_file:

	fprintf(stderr, "old_write: file type is unknown\n");
	fprintf(stderr, "old_write: file path %s\n", nm);

fail:
	fprintf(stderr, "old_write: failed to prapre file\n");
	free(file);
	return NULL;

}

static struct pfile *just_read(const char *nm)
{
	struct pfile *file = calloc(1 , sizeof(struct pfile));
	if (file == NULL)
		return NULL;

	FILE *f = fopen(nm, "rb");
	if (f == NULL)
		goto open_fail;

	file->fp = f;
	file->nm = nm;
	file->tmp = NULL;
	file->mode |= FILE_READ;

	int ret;

	if((ret = check_header(file)))
		goto read_index;

	if (ret == -1)
		goto fail;

	if (is_flag(IG_HEADER)) {
		fprintf(stderr, "just_read: unknown header but '-ih' or '-ignore-header'"
				" option is used so operation will be continue\n");
		goto read_index;
	}

	goto unknown_file;

read_index:
	ret = read_ix(file);
	if (ret == -1)
		goto fail;

	return file;

open_fail:
	perror("just_read(fopen)");
	goto fail;

unknown_file:
	fprintf(stderr, "just_read: file type is unknown\n");
	fprintf(stderr, "just_read: file path %s\n", nm);

fail:
	fprintf(stderr, "just_read: failed to open file for reading\n");
	free(file);
	return NULL;
}

static struct pfile *op_pfile(
	const char *nm,
	int mode)
{
	if (nm == NULL)
		goto eival;

	if (mode & FILE_READ)
		return just_read(nm);

	if (mode & FILE_NEW)
		return new_file(nm);

	if (mode & FILE_WRITE)
		return old_write(nm);

eival:
	fprintf(stderr, "op_pfile: invalid input\n");
	return NULL;
}

static int cls_pfile(struct pfile *file)
{
	if (file == NULL)
		return 0;

	int ret;
	size_t write;
	if (file->mode & FILE_READ)
		goto close;

	ret = set_at(file, INDEX);
	if (ret == -1)
		goto fail;

	write = fwrite(&file->index, sizeof(struct index), 1, file->fp);
	if (write < 1)
		goto write_fail;

close:
	if (is_flag(FLG_VERBOSE)) {
		printf("cls_pfile: before closing file\n");
		show_pfile(file);
	}

	ret = fclose(file->fp);
	if (ret == EOF)
		goto fclose_fail;

	if (file->mode & FILE_READ)
		goto out;

	ret = rename(file->tmp, file->nm);
	if (ret == -1)
		goto rename_fail;

	goto out;

rename_fail:
	perror("cls_pfile(rename)");
	goto fail;

write_fail:
	perror("cls_pfile(fwrite)");
	goto fail;

fclose_fail:
	perror("cls_pfile(fclsoe)");
	goto fail;

fail:
	show_pfile(file);
	empty_bucket(file->pbuk);
	free(file);
	return -1;

out:
	free(file->tmp);
	empty_bucket(file->pbuk);
	free(file);
	return 0;
}

static ssize_t parse_argv(
	char **argv,
	int invalcount,
	struct bucket *buk)
{
	if (*argv == NULL) {
		if (invalcount != 0) {
			printf("%d values were invalid \n", invalcount);
			return -1;
		}
		printf("all value were correct\n");
		return buk->write;
	}

	char *endpr = *argv;
	long long digit = strtoll(*argv, &endpr, 10);

	if (digit == 0 && endpr == *argv) {
		if (have_flag_prefix(*argv)) {
			fprintf(stderr,
				"'%s' appear like flag ingoring it\n"
				"its good to use flags after input values\n",
				*argv);

			return parse_argv(argv + 1, invalcount, buk);
		}
		fprintf(stderr, "%s is invalid\n", *argv);
		return parse_argv(argv + 1, invalcount + 1, buk);
	}

	if (*endpr == '\0') {
		struct data d;
		d.id = (uint64_t) digit;

		int ret = add_to_bucket(d, buk);
		if (ret == -1)
			return -1;

		return parse_argv(argv + 1, invalcount, buk);
	}
	printf("%s is invalid from %s\n", *argv, endpr);
	return parse_argv(argv + 1, invalcount + 1, buk);
}

static struct look_up_table *bring_table()
{
	struct look_up_table *tab = NULL;
	tab = malloc(sizeof(struct look_up_table));
	if (tab == NULL)
		return NULL;

	tab->data = NULL;
	tab->ids = NULL;
	tab->table_cap = 0;
	tab->table_cur = 0;
	return tab;

}

static int populate_table(struct bucket *buk, struct look_up_table *tab)
{
	tab->ids = malloc(sizeof(uint64_t) * buk->write);
	if (tab->ids == NULL)
		return -1;

	tab->data = buk->data;

	for (buk->read = 0; buk->read < buk->write; buk->read++) {
		memcpy(&tab->ids[buk->read],
			&buk->data[buk->read].id,
			sizeof(uint64_t));

	}

	tab->table_cur = buk->write;
	tab->table_cap = tab->table_cur;

	return 0;
}

static ssize_t get_slot(struct look_up_table *tab, uint64_t id)
{
	for (size_t i = 0; i < tab->table_cur; i++)
		if(tab->ids[i] == id)
			return i;

	return -1;
}

static struct data *look_up(struct look_up_table *tab, uint64_t id)
{
	ssize_t slot = get_slot(tab, id);
	if (slot == -1)
		return NULL;

	return &tab->data[slot];
}

static int grow_table(struct look_up_table *tab, size_t size)
{
	uint64_t *tmp = realloc(tab->ids, sizeof(uint64_t) * size);
	if (tmp == NULL)
		return -1;

	struct data *tmp_data = realloc(tab->data, sizeof(struct data) * size);
	if (tmp_data == NULL) {
		free(tmp);
		return -1;
	}

	tab->ids = tmp;
	tab->data = tmp_data;
	tab->table_cap = size;

	return 0;

}

static void break_table(struct look_up_table *tab)
{
	if (tab == NULL)
		return;

	free(tab->ids);
	free(tab);

}

static const char *get_tmp_name(const char *name)
{
	char *dup_path = strdup(name);
	if (dup_path == NULL)
		return NULL;

	char *trainling_slash = strrchr(dup_path, '/');
	if (*(trainling_slash + 1) == '\0')
		*trainling_slash = '\0';

	trainling_slash = strrchr(dup_path, '/');
	*trainling_slash = '\0';

	char *basename = trainling_slash + 1;

	char *tmp_name = NULL;
	char *tmp_path = NULL;

	int ret = asprintf(&tmp_name, ".%s", basename);
	if (ret == -1)
		goto fail;

	ret = asprintf(&tmp_path, "%s/%s", dup_path, basename);
	if (ret == -1)
		goto fail;

	if (!is_flag(FLG_VERBOSE))
		goto out;

	printf("get_tmp_name: after string manupulation this is output\n");

	printf("get_tmp_name: dup_path: %s\n", dup_path);
	printf("get_tmp_name: basename: %s\n", basename);

	printf("get_tmp_name: tmp_name: %s\n", tmp_name);
	printf("get_tmp_name: tmp_path: %s\n", tmp_path);

out:
	free(dup_path);
	free(tmp_name);

	return tmp_path;
fail:
	free(tmp_path);
	free(tmp_name);
	free(dup_path);
	return NULL;
}
static int have_flag_prefix(char *p)
{
	if (p == NULL)
		return 0;

	int c = strncmp(p, "-", 1);
	if (c == 0)
		return 1;

	return 0;
}

static int is_flag(int flag)
{
	if (blob_cmd_flags & flag)
		return 1;

	return 0;
}

static int enable_flag(char **argv)
{
	/**
	 * if caller pass NULL pointer then there is no
	 * way to use this pointer so we can consider it
	 * as error
	 */
	if (argv == NULL)
		return -1;
check_null:
	/**
	 * having no more argument in list is not error
	 * lets just return 0 to caller
	 */
	if (*argv == NULL)
		return 0;
	/**
	 * lets ignore subsequent non
	 * flag strings in argument list
	 */
	if (!have_flag_prefix(*argv)) {
		argv++;
		goto check_null;
	}

	char *opt = *argv;
	int option_found = 0;

	for (size_t i = 0; option[i].short_name != NULL; i++) {
		if (strcmp(opt, option[i].short_name) == 0) {
			blob_cmd_flags |= option[i].flag;
			option_found = 1;
			break;
		}
		if (strcmp(opt, option[i].long_name) == 0) {
			blob_cmd_flags |= option[i].flag;
			option_found = 1;
			break;
		}
	}
	/**
	 * invalid flag is not error lets just tell
	 * user and move to next argument
	 */
	if (!option_found)
		fprintf(stdout, "'%s' is not valid option\n", opt);

	argv++;
	goto check_null;
}

static int show_def()
{
	char *path_to_file = NULL;
	struct pfile *file = NULL;
	struct bucket *buk = NULL;

	int ret;
	enum plank_status pret;
	pret = get_blob_path(&path_to_file);
	if (pret != PLANK_OK) {
		ret = (int) pret;
		goto out;
	}

	ret = 0;

	printf("location used for blob : %s\n", path_to_file);

	file = op_pfile(path_to_file, FILE_READ);
	if (file == NULL) {
		ret = -1;
		goto out;
	}

	printf("data start offset : %#lx\n", file->index.start_off);
	printf("data end offset	  : %#lx\n", file->index.last_off);
	printf("total data 	  : %" PRIu64 "\n", file->index.total_data);

	ret = read_data(&buk, file);
	if (ret == -1)
		goto out;

	for (buk->read = 0; buk->read < buk->write; buk->read++)
		printf("data : %" PRIu64 "\n", buk->data[buk->read].id);
out:
	if (ret == -1)
		fprintf(stderr, "show_def: failed to show data\n");
	/**
	 * if cls_pfile() return error there is no
	 * specific action require to take
	 * just report it back to caller
	 */
	int cls = cls_pfile(file);
	if (cls != 0)
		ret = cls;

	pret = done_with_path();
	if (pret != PLANK_OK)
		ret = (int) pret;

	empty_bucket(buk);
	free(path_to_file);
	return ret;
}

static struct mount_info btrfs_top_subvol_mnt = {
	.sur_uuid = {0},
	.target = NULL,
	.type = MNT_UUID,
};

const char *blob_file_name = "pblob";
char *mnt_target = NULL;

static enum plank_status prep_mnt()
{
	enum plank_status ret = PLANK_OK;

	mnt_target = strdup("/mnt");
	if (mnt_target == NULL) {
		ret = PLANK_MEM_ERR;
		goto fail;
	}

	ret = get_root_mount_info(MNT_UUID, &btrfs_top_subvol_mnt);
	if (ret != PLANK_OK)
		goto fail;

	ret = mount_top_subvol(&btrfs_top_subvol_mnt, mnt_target);
	if (ret != PLANK_OK)
		goto fail;

	goto out;
fail:
	fprintf(stderr, "prep_mnt: failed to prepare mount\n");
	mnt_no_need_now(&btrfs_top_subvol_mnt);
	free_mnt_info(btrfs_top_subvol_mnt);
out:
	return ret;
}

static enum plank_status done_with_path()
{
	enum plank_status pret = PLANK_OK;
	pret =  mnt_no_need_now(&btrfs_top_subvol_mnt);
	if (pret != PLANK_OK)
		fprintf(stderr,
			"done_with_path: "
			"unable to umount top level subvolume\n");

	return pret;
}

static enum plank_status get_blob_path(char **p)
{
	enum plank_status ret = PLANK_OK;
	int tmpint = 0;

	ret = prep_mnt();
	if (ret != PLANK_OK)
		goto out;

	tmpint = asprintf(p, "%s/%s", mnt_target, blob_file_name);
	if (tmpint < 0)
		ret = PLANK_MEM_ERR;

out:
	if (ret != PLANK_OK)
		fprintf(stderr,
			"get_blob_path: "
			"failed to get path of blob\n");
	return ret;
}

int add(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "plank blob %s : please provide data in following format\n"
				"plank blob %s :'id' 'id' 'id' ... ", argv[0], argv[0]);
		return -1;
	}
	enable_flag(argv);

	int ret;
	char *cmd = argv[1];
	if (strcmp(cmd, "--help") == 0) {
		add_usage();
		return 1;
	}
	struct bucket *buk_w = NULL;
	struct bucket *buk_1 = NULL;
	struct pfile *file = NULL;
	char *path_to_file = NULL;

	buk_1 = bring_bucket();
	if (buk_1 == NULL)
		return -1;

	ssize_t values = parse_argv(argv + 1, 0, buk_1);
	if (values == -1) {
		fprintf(stderr, "please provide data in only digits\n");
		goto out;
	}

	enum plank_status pret = PLANK_OK;
	pret = get_blob_path(&path_to_file);
	if (pret != PLANK_OK) {
		ret = (int) pret;
		goto out;
	}

	ret = access(path_to_file, F_OK);
	if (ret == 0) {
		file = op_pfile(path_to_file, FILE_WRITE);
		if (file == NULL) {
			ret = -1;
			goto out;
		}

		printf("file name : %s\n", path_to_file);

		ret = 0;
		goto merge_buk;
	}
	file = op_pfile(path_to_file, FILE_WRITE | FILE_NEW);
	if (file == NULL) {
		ret = -1;
		goto out;
	}
	ret = write_header(file);
	if (ret == -1)
		goto out;

	ret = write_ix(file);
	if (ret == -1)
		goto out;

	buk_w = buk_1;
	goto write_data;

merge_buk:
	buk_w = do_bucket(file->pbuk, buk_1, BUK_MERGE);
	if (buk_w == NULL)
		goto out;

	show_buk(buk_w);
write_data:
	ret = write_data(buk_w, file);
	if (ret == -1)
		goto out;
out:
	if (ret == -1)
		fprintf(stderr, "failed to write proper binary blob\n");

	int cls = cls_pfile(file);
	if (cls != 0)
		ret = cls;

	free(path_to_file);

	if (buk_1 == buk_w) {
		empty_bucket(buk_1);
	} else {
		empty_bucket(buk_1);
		empty_bucket(buk_w);
	}

	pret = done_with_path();
	if (pret != PLANK_OK)
		ret = (int) pret;

	return ret;
}

int show(int argc, char **argv)
{
	if (argc < 2)
		goto just_show;

	char *opt = argv[1];
	if (strcmp(opt, "--help") == 0) {
		show_usage();
		return 1;
	}

	enable_flag(argv);

just_show:

	return show_def();
}

int rm(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr,
			"blob %s: please provide data in following format.\n"
			"blob %s: 'blob %s 'id' 'id' 'id' ...\n",
			argv[0],
			argv[0], argv[0]);

		return -1;
	}

	if (have_flag_prefix(argv[1])) {
		if (strcmp(argv[1], "--help") == 0) {
			rm_usage();
			return -1;
		}
	}

	enable_flag(argv);

	int ret = 0;

	char *file_path = NULL;

	struct bucket *buk_1 = NULL;
	struct bucket *buk_w = NULL;

	struct pfile *file = NULL;

	enum plank_status pret = PLANK_OK;
	pret = get_blob_path(&file_path);
	if (pret != PLANK_OK) {
		ret = (int) pret;
		goto out;
	}

	buk_1 = bring_bucket();
	if (buk_1 == NULL) {
		ret = -1;
		goto out;
	}

	ssize_t values = parse_argv(argv + 1, 0, buk_1);
	if (values == -1) {
		fprintf(stderr, "please only pass digits\n");
		ret = -1;
		goto out;
	}

	file = op_pfile(file_path, FILE_WRITE);
	if (file == NULL) {
		ret = -1;
		goto out;
	}

	buk_w = do_bucket(file->pbuk, buk_1, BUK_SUB);
	if (buk_w == NULL)
		goto out;

	ret = write_data(buk_w, file);
	if (ret == -1)
		goto out;

out:
	if (ret != 0)
		fprintf(stderr, "failed to remove data from binary blob\n");

	int cls = cls_pfile(file);
	if (cls != 0)
		ret = cls;

	pret = done_with_path();
	if (pret != PLANK_OK)
		ret = (int) pret;

	empty_bucket(buk_1);
	empty_bucket(buk_w);
	return ret;
}

/**
 * it expose functionality while keeping most
 * function static
 */

extern int merge_buk(
	struct bucket *buk1,
	struct bucket *buk2,
	struct bucket **ret)
{
	*ret = do_bucket(buk1, buk2, BUK_MERGE);
	if (*ret == NULL)
		return -1;

	return 0;
}

extern int get_blob_data(struct bucket **buk_ret)
{
	char *path_to_file = NULL;
	struct pfile *file = NULL;
	struct bucket *buk = NULL;

	int ret;
	enum plank_status pret;

	pret = get_blob_path(&path_to_file);
	if (pret != PLANK_OK) {
		ret = (int) pret;
		goto out;
	}

	ret = 0;
	file = op_pfile(path_to_file, FILE_READ);
	if (file == NULL) {
		ret = -1;
		goto out;
	}

	ret = read_data(&buk, file);
	if (ret == -1)
		goto out;

out:
	if (ret == -1) {
		empty_bucket(buk);
		buk = NULL;
	}

	cls_pfile(file);

	pret = done_with_path();
	ret = (int) pret;

	free(path_to_file);
	*buk_ret = buk;

	return ret;

}

void blob_usage()
{
	printf("plank blob:\n"
		"add	: add given id to keep\n"
		"show	: show current data stored\n"
		"remove : remove data from file\n"
		"use 'command' --help to find help about command\n");
}

static void show_usage()
{
	printf("plank blob show : show data that is currectly stored by\n"
		"		  plank in its binary blob\n"
		"following options can be used to change behaviour of program or for debuging purpose\n"
		"-ih, -ignore-header	:ingore header of file and continue operation\n");
}

static void add_usage()
{
	printf("pass ids in following format\n"
		"plank blob add 'id' 'id' 'id' ...\n");
}

static void rm_usage()
{
	printf("plank blob remove: remove data from blob\n"
		"use --help for help\n");

}
