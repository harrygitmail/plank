#pragma once
#include "types.h"
#include <stddef.h>

extern const char *const entr_time_format;

struct loader_entries *bring_ld_entrs(size_t entries_count);
void free_ld_entrs(struct loader_entries *entrs);

struct loader_entries_w *bring_ld_entrs_w(size_t entries);
void free_ld_entrs_w(struct loader_entries_w *entrs_w);

/**
 * entr_ks:- allocate entries and setup kernel version and snapshot info
 * 	     for each entrie.
 *
 * NOTE:-    it does not set status of entrie and give filename. you need
 * 	     to use 'entr_mark' and 'entr_give_file_name'. it also do not
 * 	     set other field such as entry_token and pretty_name
 * 	     caller suppose to set those according their needs.
 */
struct loader_entries *entr_ks(
	struct kernel_list *klist,
	struct snapshot_list *slist);

/**
 * entr_give_file_name:- give filename to existing entries in @entries
 *
 * if @entries have corrupted data then behaviour is undefined.
 * for example entries before 'entries->counts' having missing kernel info
 * and snapshot info.
 */
int entr_give_file_name(struct loader_entries *entries, char *entrie_token);

/**
 * entr_mark:- update status of entrie
 *
 * @index: index tells which entries need to be set status as @status
 *
 * if @index is null it will update all entries status to @status
 */
void entr_mark(
	struct loader_entries *entrs,
	entry_status status,
	const int *index);

/**
 * entr_up_p:- update internal pointer of object @entrs.
 *
 * @entrs:- object that hold entries
 * @p:- pointer to assign to entries in @entrs
 * @index:- represent which entries require to be updated
 * @which:- which pointer to update, refer to below macros
 */

#define		P_ENTR_TOKEN	0x1001	/* entry-token */
#define		P_ID		0x1002  /* id (from os-release) */
#define		P_SUR_UUID	0x1003 	/* source uuid */
#define		P_PRETTY_NAME	0x1004	/* pretty name (from os-release) */

void entr_up_p(
	struct loader_entries *entrs,
	char *p,
	const int *index,
	int which);

/**
 * entr_add:- add entrie to already exsisting @entries
 */
int entr_add(
	struct loader_entries *entries,
	struct loader_entry entrie);


int entr_w_add(
	struct loader_entries_w *entrs_w,
	struct loader_entry_w entr);

/**
 * entr_to_etr_w:- preapre entries for writing
 *
 * NOTE:- it may return 0 if @entr is not marked as "ENTRY_MAKE_PENDING"
 * 	  and may do nothing to @entr_w. caller is responsibe for allocating
 * 	  @entr_w. 'entr_to_entr_w' just allocat its internal data and update
 * 	  pointers in @entr_w. caller is responsibe to free those memory.
 *
 * return -1 on error, and 0 on success.
 * caller should not use any data of @entr_w if there is error
 * because @entr_w may contain garbage or wrong pointer
 */

int entr_to_entr_w(
	struct loader_entry *entr,
	struct loader_entry_w *entr_w);

/**
 * entr_w_to_entr:- turn parsed entry from filesystem to 'loader_entrie'
 * 		    object
 *
 * NOTE:-'entr_to_entr_w' assume @entr have valid data and for now there
 * 	is no checks for @entr caller should allocate @entr. internal data
 * 	will be allocated by  'entr_w_to_entr' and caller should it free
 * 	after no longer in use.
 */
int entr_w_to_entr(
	struct loader_entry_w *entr_w,
	struct loader_entry *entr);

/**
 * entr_give_id:- give entry id to given entry by its internal data
 *
 * NOTE:- 'entr_give_id' assume given @entr is valid and it contain all
 * 	   data. behaviour is undefined if caller give partially corrputed
 * 	   data or invalid pointer.
 *
 * 	   @entr internal data could have null pointer and it will be
 * 	   ignored by 'entr_give_id' for now.
 */

int entr_give_id(struct loader_entry *entr);

#define ENTR_ENTRY_TOKEN	(1 << 0)
#define ENTR_ID			(1 << 1)
#define ENTR_PRETTY_NAME	(1 << 2)
#define ENTR_SUR_UUID		(1 << 3)
#define ENTR_KERN       	(1 << 4)
#define ENTR_SNAP		(1 << 5)

/**
 * entr_is_equal: compare internal data of two entry
 *
 * return 1 if compared data is equal, zero if not equal, -1 on error.
 */
int entr_is_equal(
	struct loader_entry *entr1,
	struct loader_entry *entr2,
	int what);
