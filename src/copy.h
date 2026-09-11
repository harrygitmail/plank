
/**
 * copy_file:- copy file from '@src_path' to '@dest_path'
 *
 * @src_path : refer to intput file.
 * @dest_path: refer to path of output file. if it do not exist
 * 	       copy_file will create it.
 *
 * BUG: when @src_path refer to directory then 'sendfile()' will fail
 * 	with EINVAL because O_CREATE flag were set on output fd.
 */

int copy_file(char *src_path, char *dest_path);
