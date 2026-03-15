#include "./dynamic_hpack_table.h"

NODISCARD HpackHeaderDynamicTable hpack_dynamic_table_get_empty(void) {
	return (HpackHeaderDynamicTable){
		.entries = NULL,
		.count = 0,
		.capacity = 0,
		.start = 0,
	};
}

NODISCARD const HpackHeaderDynamicEntry*
hpack_dynamic_table_at(const HpackHeaderDynamicTable* const dynamic_table, const size_t index) {
	if(index >= dynamic_table->count) {
		return NULL;
	}

	size_t idx = (dynamic_table->start + index) % dynamic_table->capacity;

	return &(dynamic_table->entries[idx]);
}

NODISCARD size_t hpack_dynamic_table_size(const HpackHeaderDynamicTable* const dynamic_table) {
	return dynamic_table->count;
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

	*dynamic_table = hpack_dynamic_table_get_empty();
}

#define IMPL_GROWTH_FACTOR(cap) ((cap) == 0 ? 32 : (cap) * 2)

NODISCARD HpackHeaderDynamicEntry*
hpack_dynamic_table_pop_at_end(HpackHeaderDynamicTable* const dynamic_table) {

	if(dynamic_table->count == 0) {
		return NULL;
	}

	size_t last_idx = (dynamic_table->start + dynamic_table->count - 1) % dynamic_table->capacity;

	dynamic_table->count--;

	// TODO(Totto): if we are small enough, resize and free some things!

	return &(dynamic_table->entries[last_idx]);
}

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

		{ // adjust the layout
			// layout before [0 1 2 3 4 5]
			//       start here   ^

			// so after it need to be reorder like this
			// layout before [x x 2 3 4 5 0 1 x x]
			//       start here   ^       ^ new items here

			// if start == 0, no reordering is needed
			if(dynamic_table->start != 0) {
				const size_t amount = dynamic_table->start;
				for(size_t i = 0; i < amount; ++i) {
					const size_t target_idx = dynamic_table->count + i;

					dynamic_table->entries[target_idx] = dynamic_table->entries[i];
#ifndef NDEBUG
					{ // only for debug purposes, reset to tstr_null entries
						dynamic_table->entries[i] = (HpackHeaderDynamicEntry){
							.key = tstr_null(),
							.value = tstr_null(),
						};
					}
#endif
				}
			}
		}

#ifndef NDEBUG
		{ // for debugging purpose, fill the unused entries with tstr_null values, as all 0s is a
			// valid SSO tstr, that signifies empty

			const size_t free_amount = dynamic_table->capacity - dynamic_table->count;
			assert(free_amount > 0);

			const size_t start_idx =
			    (dynamic_table->start + dynamic_table->count) % dynamic_table->capacity;

			for(size_t i = 0; i < free_amount; ++i) {
				const size_t idx = (start_idx + i) % dynamic_table->capacity;

				dynamic_table->entries[idx] = (HpackHeaderDynamicEntry){
					.key = tstr_null(),
					.value = tstr_null(),
				};
			}
		}

#endif
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
