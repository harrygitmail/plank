#include "types.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

enum plank_status write_entrie(const loader_entry_w *entrie,
	FILE *steam)
{
	enum plank_status ret = PLANK_OK;
	const char *entrie_type = "# Boot Loader Specification type #1 entry";
	const char *warning = "# entrie made by 'plank' and may not work";

	fprintf(steam, "%s\n", entrie_type);
	fprintf(steam, "%s\n", warning);

	if(fprintf(steam, "title      %s\n", entrie->title) < 0)
		goto fail;

	if(fprintf(steam, "version    %s\n", entrie->version) < 0)
		goto fail;

	if(fprintf(steam, "machine-id %s\n", entrie->machine_id) < 0)
		goto fail;

	if(fprintf(steam, "sort-key   %s\n", entrie->sort_key) < 0)
		goto fail;

	if(fprintf(steam, "options    %s\n", entrie->options) < 0)
		goto fail;

	if(fprintf(steam, "linux      %s\n", entrie->kernel) < 0)
		goto fail;

	if(fprintf(steam, "initrd     %s\n", entrie->initrd) < 0)
		goto fail;

	goto out;

fail:
	ret = PLANK_ERR;

out:

	return ret;
}
