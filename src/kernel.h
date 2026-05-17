#pragma once
#include <types.h>

/* get_kernel_list - get list of kernel installed on system
 *
 * @ret: pointer to 'kernel_list' array head.
 *
 * BUG: it only look into ditectory names it found in "/lib/modules/"
 * so if there is random directory in "/lib/modules/" it may treat it as valid
 * kernel version even if it do not contain any Linux kernel image.
 * possible solution could be to check exsistance of Linux kernel image in that 
 * folder but it may make one function doing lots of work.
 */
enum plank_status get_kernel_list(struct kernel_list *const ret, const char *root);

enum kern_diff {
	KERN_P1,  		//kernel present in first list
	KERN_P2,		//kernel present in secound list
	KERN_COM		//kernel present in both list
};

struct kernel_list *kernel_diff(
	const struct kernel_list *k1, 
	const struct kernel_list *k2,
	enum kern_diff diff_type);
