#include <sys/types.h>
#include <stdio.h>

/* loader_entry - store pointers to memeory that hold specific info
 *
 * plank is in very initial stage so loader_entry_w defination may chnage 
 * in future for batter performace and less headache of managing memory.
 */


typedef struct {

	char *filename;
	char *title;
	char *version;
	char *machine_id;
	char *sort_key;
	char *options;
	char *kernel;
	char *initrd;

} loader_entry_w;

char **read_conf_file(FILE *file);
