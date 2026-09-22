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
