#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>

static int get_file_size(int fd, off_t *size);

int copy_file(char *src_path, char *dest_path)
{
	int in_fd;
	int out_fd;
	int ret;
	off_t offset = 0;
	off_t file_size = 0;
	size_t count = 0;
	ssize_t write = 0;
	char *ppath = NULL;

	in_fd = open(src_path, O_RDONLY);
	if (in_fd == -1) {
		ppath = src_path;
		goto open_fail;
	}

	out_fd = open(dest_path, O_RDWR | O_CREAT);
	if (out_fd == -1) {
		ppath = dest_path;
		goto open_fail;
	}

	ret = get_file_size(in_fd, &file_size);
	if (ret < 0)
		goto fail;

	count = (size_t) file_size;
	write = sendfile(out_fd, in_fd, &offset, count);
	if (write < 0) {
		perror("copy_file(sendfile)");
		goto fail;
	}

	if (offset < file_size)
		goto not_comp;

	return 0;

open_fail:
	fprintf(stderr,
		"copy_file(open): '%s' %s\n", ppath, strerror(errno));
	goto fail;

not_comp:
	fprintf(stderr,
		"copy_file: could not write file fully\n");
fail:
	fprintf(stderr,
		"copy_file: failed to copy file\n");
	return -1;
}

static int get_file_size(int fd, off_t *size)
{
	if (size == NULL)
		goto inval;

	if (fd < 0)
		goto inval;

	struct stat st;
	int ret = 0;

	ret = fstat(fd, &st);
	if (ret < 0) {
		perror("get_file_size(fstat)");
		goto fail;
	}

	*size = st.st_size;
	return 0;
inval:
	fprintf(stderr,
		"get_file_size: invalid argument\n");
	goto fail;

fail:
	fprintf(stderr,
		"get_file_size: failed to get file size\n");
	return -1;
}
