#pragma once

#include "utils/utils.h"

typedef struct {
	char* key;
	char* value;
} HpackHeaderDynamicEntry;

// We need those three properties for this "container":
// 1: O(1) indexing
// 2: O(1) insertion at the start
// 3: O(1) deleting at the end
// arrays only provide 1 and 3, a (double) linked list only 2 and 3
//
// so this is a cyclic buffer:
// 1 is achieved by indexing at (start + i) % capacity, so it is constant, but not linear!
// 2 is achieved by inserting at the (start - 1) % capacity index, which might wrp around
// 3 is achieved by deleting the (start + count - 1) % capacity index
typedef struct {
	HpackHeaderDynamicEntry* entries;
	size_t count;
	size_t capacity;
	//
	size_t start;
} HpackHeaderDynamicTable;

NODISCARD HpackHeaderDynamicTable hpack_dynamic_table_empty(void);

NODISCARD HpackHeaderDynamicEntry
hpack_dynamic_table_at(const HpackHeaderDynamicTable* dynamic_table, size_t index);

void free_dynamic_entry(HpackHeaderDynamicEntry entry);

void hpack_dynamic_table_free(HpackHeaderDynamicTable* dynamic_table);

NODISCARD HpackHeaderDynamicEntry*
hpack_dynamic_table_pop_at_end(HpackHeaderDynamicTable* dynamic_table);

NODISCARD bool hpack_dynamic_table_insert_at_start(HpackHeaderDynamicTable* dynamic_table,
                                                   HpackHeaderDynamicEntry entry);
