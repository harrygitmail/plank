#pragma once

/* this  command is for testing logic please do not use otherwise.
 */
int show_host_info(int argc, char **argv);


/* make_entry - make entry for snapshot it found.
 *
 * it make boot loader type #1 entry for snapshot with all kernels and initrd
 * it currently do not make entry for initrd-fallback or any other user specified 
 * custome image or image are on $BOOT.
 */

int make_entrie(int argc, char **argv);


int clean(int argc, char **argv);

int show_entrie(int argc, char **argv);

int check(int argc, char **argv);

int ls_subvol(int argc, char **argv);

int blob(int argc, char **argv);
