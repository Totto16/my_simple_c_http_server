#include "./dynamic_hpack_table.h"

NODISCARD HpackHeaderDynamicTable hpack_dynamic_table_empty(void) {
	return (HpackHeaderDynamicTable){
		.entries = NULL,
		.count = 0,
		.capacity = 0,
		.start = 0,
	};
}

NODISCARD HpackHeaderDynamicEntry
hpack_dynamic_table_at(const HpackHeaderDynamicTable* const dynamic_table, const size_t index) {
	assert(index < dynamic_table->count && "Dynamic table index out of bounds!");

	size_t idx = (dynamic_table->start + index) % dynamic_table->capacity;

	return dynamic_table->entries[idx];
}

void free_dynamic_entry(HpackHeaderDynamicEntry entry) {
	tstr_free(&entry.key);
	tstr_free(&entry.value);
}

void hpack_dynamic_table_free(HpackHeaderDynamicTable* const dynamic_table) {
	for(size_t i = dynamic_table->start, rest_count = dynamic_table->count; rest_count != 0;
	    i = (i + 1) % dynamic_table->capacity, rest_count--) {
		HpackHeaderDynamicEntry entry = dynamic_table->entries[i];

		free_dynamic_entry(entry);
	}

	dynamic_table->count = 0;

	free(dynamic_table->entries);

	*dynamic_table = hpack_dynamic_table_empty();
}

NODISCARD HpackHeaderDynamicEntry*
hpack_dynamic_table_pop_at_end(HpackHeaderDynamicTable* const dynamic_table) {

	if(dynamic_table->count == 0) {
		return NULL;
	}

	size_t last_idx = (dynamic_table->start + dynamic_table->count - 1) % dynamic_table->capacity;

	dynamic_table->count--;

	return &(dynamic_table->entries[last_idx]);
}

#define IMPL_GROWTH_FACTOR(cap) ((cap) == 0 ? 32 : (cap) * 2)

NODISCARD bool hpack_dynamic_table_insert_at_start(HpackHeaderDynamicTable* const dynamic_table,
                                                   HpackHeaderDynamicEntry entry) {

	if(dynamic_table->count >= dynamic_table->capacity) {

		const size_t new_capacity = IMPL_GROWTH_FACTOR(dynamic_table->capacity);

		HpackHeaderDynamicEntry* new_entries = (HpackHeaderDynamicEntry*)realloc(
		    (void*)dynamic_table->entries, new_capacity * sizeof(HpackHeaderDynamicEntry));
		if(!new_entries) {
			return false;
		}
		dynamic_table->entries = new_entries;
		dynamic_table->capacity = new_capacity;
	}

	// don't just subtract 1, as it may underflow and the size_t + the c % operation would render
	// that an invalid value, so just add the capacity first, so that it is always >= 0 before
	// modulo
	size_t first_free_idx =
	    (dynamic_table->start + dynamic_table->capacity - 1) % dynamic_table->capacity;

	dynamic_table->start = first_free_idx;

	dynamic_table->entries[first_free_idx] = entry;
	dynamic_table->count++;

	return true;
}
