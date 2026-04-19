#include "commands.h"
#include <stdio.h>
#include <string.h>

typedef struct {
	const char *name;
	int (*func)(int argc, char **argv);
} command;

const char *program_name = "plank";

command commands[] = {
	{"show-host-info", show_host_info},
	{"make-entry", make_entry},
	{"clean", clean},
	{"list-entry", list_entry},
	{NULL,NULL}
};

void usage()
{

	printf("plank is in initial stage of development.\n"
		"please do not use it on your system.\n"
		"show-host-info: to show info about host\n"
		"make-entry: to make systemd-boot entry\n");
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		usage();
		return 1;
	}

	char *cmd_name = argv[1];

	for (int i=0; commands[i].name != NULL; i++) {

		if (strcmp(cmd_name, commands[i].name) == 0) {

			return commands[i].func(argc - 1, &argv[1]);
		}
	}

	printf("%s: Unknow command %s\n", argv[0], cmd_name);
	usage();

	return 1;
}
