#pragma once
#include "types.h"

void empty_buk(struct bucket *buk);
int grow_buk(struct bucket *buk, size_t size);
struct bucket *op_buk(const char *path, int type);
int add_to_buk(struct bucket *buk, size_t items, void *data);

/**
 * right now only can convert BUK_TYPE_DATA to BUK_TYPE_SNAP
 * leaving @type for future extension. and will be ignored
 * by function. argv @type can be remove if we really
 * do not need to converte it in others types.
 */
struct bucket *convt_buk(struct bucket *buk, int type);
