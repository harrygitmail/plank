#include <stdio.h>
#include "types.h"

/**
 * parse_entry:-  parse entry from given file stream
 *
 * if @file is null and @path is non-null then 'parse_entry'
 * will open file stream at @path and will parse entry.
 *
 * if @file is non-null and @path is null then 'parse_entry'
 * will start reading from @file and return entry. note that
 * 'parse_entry' do not initially reset file position it will
 * start reading from wherever it was when caller passed pointer.
 * it also do not setup filename as path were not passed. caller
 * should close @fl in this case.
 *
 * if both are non-null then effect is same as above expect that
 * 'parse_entry' will update filename field. caller should close
 * @fl.
 *
 * if both are null then 'parse_entry' will return null.
 */

struct loader_entry_w *parse_entry(FILE *fl, const char *path);
/**
 * parse_option:- parse option from option string
 *
 * @op: option string
 * @target: target to parse (look above marcors)
 *
 * 'parse_option' will allocate enough memory to hold string and update
 * pointer of caller. caller is responsible for freeing this memory.
 *
 * NOTE:- 'parse_option' do not force type(like uint64 for subvolid).
 * 	   caller suppose to convert to appropirate type it need.
 * 	  'parse_option' will just return result in form of string
 */

#define OP_UUID		0x10
#define OP_SUBVOL_ID	0x20

int parse_option(const char *op, int target, char **ret);

/**
 * parse_title:- parse different field from title string
 *
 * NOTE:- 'parse_title' assume string is in following form
 *
 * 	snapshot EndeavourOS 2026-08-02 00:05:17 6.18.44-1-lts
 *
 * 	as this format is followed by 'plank' for writing loader
 * 	entry any title string associated with loader entry
 * 	created by plank will have this format.
 *
 * 	again it return pointer to string caller, is responsible
 * 	to turn it into type it need. caller should free it if it no
 * 	longer in use.
 */

#define TITLE_PRETTY_NAME		0xa1
#define TITLE_KERN_VER			0xa2
#define TITLE_SNAP_TIME			0xa3

int parse_title(const char *title, int target, char **ret);

/**
 * parse_id_skey: parse id from sort-key string
 */
int parse_id_skey(char **ret_str, const char *sort_key);

/**
 * bsname:- give filename in path
 *
 * example: @path -> /home/harry/test.c
 * 	    return pointer -> test.c
 *
 * NOTE:- currently it return pointer within same memory block it passd
 * 	  by @path so be carefull using returned pointer.
 */
const char *bsname(const char *path);
