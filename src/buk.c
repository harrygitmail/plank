#include "types.h"
#include "buk.h"
#include <stdlib.h>
#include <unistd.h>

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
