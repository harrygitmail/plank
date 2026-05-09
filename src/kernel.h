#pragma once
#include "btrfs.h"
/* kernel_list - store version string of Linux kernel
 *
 * @kernel_ver: hold version string
 *
 * why 65 ? in 'struct new_utsname' from 'include/uapi/linux/utsname.h' (Linux kernel source tree),
 * for most system today its enought unless our binary 
 * running on 1991 toaster or a custom NASA space-probe
 */

struct kernel_ver {
	char kernel_ver[65];
};

#include <stddef.h>
typedef struct {
	struct kernel_ver *list;
	size_t counts;

} kernel_list;

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
enum plank_status get_kernel_list(kernel_list *const ret, const char *root);

kernel_list *kernel_diff(const kernel_list *k1, const kernel_list *k2);
