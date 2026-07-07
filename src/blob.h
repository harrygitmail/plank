#pragma once
#include "types.h"

int add(int argc, char **argv);

int show(int argc, char **argv);

int rm(int argc, char **argv);

void blob_usage();

extern int merge_bucket(
	struct bucket *buk1,
	struct bucket *buk2,
	struct bucket **ret);

extern int get_blob_data(struct bucket **ret_buk);
