#include "./dynamic_hpack_table.h"

NODISCARD HpackHeaderDynamicTable hpack_dynamic_table_get_empty(void) {
	return (HpackHeaderDynamicTable){
		.entries = NULL,
		.count = 0,
		.capacity = 0,
		.start = 0,
	};
}

NODISCARD HpackHeaderDynamicEntryResult
hpack_dynamic_table_at(const HpackHeaderDynamicTable* const dynamic_table, const size_t index) {
	if(index >= dynamic_table->count) {
		return (HpackHeaderDynamicEntryResult){ .ok = false,
			                                    .entry = (HpackHeaderDynamicEntry){
			                                        .key = tstr_null(), .value = tstr_null() } };
	}

	size_t idx = (dynamic_table->start + index) % dynamic_table->capacity;

	return (HpackHeaderDynamicEntryResult){ .ok = true, .entry = dynamic_table->entries[idx] };
}

NODISCARD size_t hpack_dynamic_table_size(const HpackHeaderDynamicTable* const dynamic_table) {
	return dynamic_table->count;
}

void free_dynamic_entry(HpackHeaderDynamicEntry* const entry) {
	tstr_free(&(entry->key));
	tstr_free(&(entry->value));
}

#define DYNAMIC_HPACK_TABLE_FORTIFIED _SIMPLE_SERVER_DYNAMIC_HPACK_TABLE_FORTIFIED

#if !defined(DYNAMIC_HPACK_TABLE_FORTIFIED) || \
    (DYNAMIC_HPACK_TABLE_FORTIFIED != 0 && DYNAMIC_HPACK_TABLE_FORTIFIED != 1)
	#error "DYNAMIC_HPACK_TABLE_FORTIFIED not defined or wrong value"
#endif

#if DYNAMIC_HPACK_TABLE_FORTIFIED == 1
static void assert_is_null_entry(const HpackHeaderDynamicEntry entry) {
	assert(tstr_is_null(&entry.key));

	assert(tstr_is_null(&entry.value));
}

MAYBE_UNUSED static void print_table(const HpackHeaderDynamicTable* table) {

	const size_t bound_lower = table->start;
	const size_t bound_upper = (table->start + table->count) % table->capacity;

	fprintf(stderr,
	        "Table: start: %zu size: %zu capacity: %zu, bound_lower: %zu bound_upper: %zu\n",
	        table->start, table->count, table->capacity, bound_lower, bound_upper);

	for(size_t i = 0; i < table->capacity; ++i) {

		const HpackHeaderDynamicEntry entry = table->entries[i];

		fprintf(stderr, "entry at raw idx %zu: key: " TSTR_FMT " value: " TSTR_FMT "|", i,
		        tstr_is_null(&entry.key) ? 6 : (int)tstr_len(&(entry.key)),
		        tstr_is_null(&entry.key) ? "<null>" : tstr_cstr(&(entry.key)),
		        TSTR_FMT_ARGS(entry.value));

		size_t idx = 0;
		bool is_idx = false;

		if(bound_lower < bound_upper) {
			is_idx = i >= bound_lower && i < bound_upper;
			idx = i - bound_lower;
		} else {
			is_idx = i >= bound_lower || i < bound_upper;
			idx = i >= bound_lower ? i - bound_lower : (table->capacity - bound_lower) + i;
		}

		if(is_idx) {
			fprintf(stderr, " is valid ar idx: %zu\n", idx);
		} else {
			fprintf(stderr, " is out of used area\n");
		}
	}

	fprintf(stderr, "END-------------------------------------------\n");
}

#endif

void hpack_dynamic_table_free(HpackHeaderDynamicTable* const dynamic_table) {
	for(size_t i = dynamic_table->start, rest_count = dynamic_table->count; rest_count != 0;
	    i = (i + 1) % dynamic_table->capacity, rest_count--) {
		HpackHeaderDynamicEntry* const entry = &(dynamic_table->entries[i]);

		free_dynamic_entry(entry);
	}

	dynamic_table->count = 0;

#if DYNAMIC_HPACK_TABLE_FORTIFIED == 1
	{ // only for debug purposes, check if everything is tstr_null
		for(size_t i = 0; i < dynamic_table->capacity; ++i) {
			const HpackHeaderDynamicEntry entry = dynamic_table->entries[i];

			assert_is_null_entry(entry);
		}
	}
#endif

	free(dynamic_table->entries);

	*dynamic_table = hpack_dynamic_table_get_empty();
}

#define IMPL_START_SIZE (32UL)
#define IMPL_GROWTH_FACTOR (2UL)
#define IMPL_SHRINK_FACTOR (IMPL_GROWTH_FACTOR * IMPL_GROWTH_FACTOR)

#define IMPL_CALC_GROW(cap) ((cap) == 0 ? IMPL_START_SIZE : (cap) * IMPL_GROWTH_FACTOR)
#define IMPL_SHOULD_SHRINK(count, cap) (count) < ((cap) / IMPL_SHRINK_FACTOR)

#define IMPL_CALC_SHRINK(cap) ((cap) / IMPL_GROWTH_FACTOR)

NODISCARD HpackHeaderDynamicEntryResult
hpack_dynamic_table_pop_at_end(HpackHeaderDynamicTable* const dynamic_table) {

	if(dynamic_table->count == 0) {
		return (HpackHeaderDynamicEntryResult){ .ok = false,
			                                    .entry = (HpackHeaderDynamicEntry){
			                                        .key = tstr_null(), .value = tstr_null() } };
	}

	size_t last_idx = (dynamic_table->start + dynamic_table->count - 1) % dynamic_table->capacity;
	const HpackHeaderDynamicEntryResult result = { .ok = true,
		                                           .entry = (dynamic_table->entries[last_idx]) };

	// don't free, the caller has to free the result!

	dynamic_table->count--;

#if DYNAMIC_HPACK_TABLE_FORTIFIED == 1
	{ // only for debug purposes, set the retrieved entry to null
		dynamic_table->entries[last_idx] = (HpackHeaderDynamicEntry){
			.key = tstr_null(),
			.value = tstr_null(),
		};
	}
#endif

	if(IMPL_SHOULD_SHRINK(dynamic_table->count, dynamic_table->capacity)) {

		const size_t new_capacity = IMPL_CALC_SHRINK(dynamic_table->capacity);

		if(new_capacity < IMPL_START_SIZE) {
			goto no_shrink;
		}

		// shrink the capcity, only shrink if we have 4 times the capcity, but we shrink it by 2, so
		// leaving enough room, that another insert doesn't need another reallocation
		assert(new_capacity != 0);

		// first reallocate all entries into the first new_capacity entries
		if(dynamic_table->start >= new_capacity) {
			// start in the soon to be removed portion, realloc to the proper start

			// do from the end, as the entries might overlap at the start, so if we fill them from
			// behind, we don't overwrite anything
			for(size_t i = dynamic_table->count; i != 0; --i) {
				const size_t old_idx = (dynamic_table->start + i - 1) % dynamic_table->capacity;
				const HpackHeaderDynamicEntry old_entry = dynamic_table->entries[old_idx];

				const size_t new_idx = i - 1;

				assert(new_idx < dynamic_table->count && new_idx < new_capacity);

#if DYNAMIC_HPACK_TABLE_FORTIFIED == 1
				assert_is_null_entry(dynamic_table->entries[new_idx]);
#endif

				assert(old_idx != new_idx);

				dynamic_table->entries[new_idx] = old_entry;

#if DYNAMIC_HPACK_TABLE_FORTIFIED == 1
				{ // only for debug purposes, reset to tstr_null entries
					dynamic_table->entries[old_idx] = (HpackHeaderDynamicEntry){
						.key = tstr_null(),
						.value = tstr_null(),
					};
				}
#endif
			}

			dynamic_table->start = 0;

		} else if(dynamic_table->start == 0) {
			// nothing to do
		} else {
			// we have to start inserting from the front, as we otherwise would overwrite existing
			// entries
			for(size_t i = 0; i < dynamic_table->count; ++i) {

				// SHOULD NEVER wrap around, logically (it is count long, start before the half, so
				// it should never reach the wraparound case)
				assert((dynamic_table->start + i) < dynamic_table->capacity);
				const size_t old_idx = (dynamic_table->start + i) % dynamic_table->capacity;
				const HpackHeaderDynamicEntry old_entry = dynamic_table->entries[old_idx];

				const size_t new_idx = i;

				assert(new_idx < dynamic_table->count && new_idx < new_capacity);

#if DYNAMIC_HPACK_TABLE_FORTIFIED == 1
				assert_is_null_entry(dynamic_table->entries[new_idx]);
#endif

				assert(old_idx != new_idx);

				dynamic_table->entries[new_idx] = old_entry;

#if DYNAMIC_HPACK_TABLE_FORTIFIED == 1
				{ // only for debug purposes, reset to tstr_null entries
					dynamic_table->entries[old_idx] = (HpackHeaderDynamicEntry){
						.key = tstr_null(),
						.value = tstr_null(),
					};
				}
#endif
			}

			dynamic_table->start = 0;
		}

		// request the realloc, if it fails, we have a problem, we just set the capcity and
		// leave the memory area bigger, but a realloc with a smaller size should never fail!
		HpackHeaderDynamicEntry* new_entries = (HpackHeaderDynamicEntry*)realloc(
		    (void*)dynamic_table->entries, new_capacity * sizeof(HpackHeaderDynamicEntry));

		if(new_entries == NULL) {
			UNREACHABLE();
		}

		dynamic_table->entries = new_entries;
		dynamic_table->capacity = new_capacity;

#if DYNAMIC_HPACK_TABLE_FORTIFIED == 1
		{ // only for debug purposes, check if all non used entires are null

			const size_t free_size = dynamic_table->capacity - dynamic_table->count;
			const size_t after_end =
			    (dynamic_table->start + dynamic_table->count) % dynamic_table->capacity;

			for(size_t i = 0; i < free_size; ++i) {
				const size_t idx = (after_end + i) % dynamic_table->capacity;
				const HpackHeaderDynamicEntry entry = dynamic_table->entries[idx];

				assert_is_null_entry(entry);
			}
		}
#endif

		//
	}

no_shrink:
	return result;
}

NODISCARD bool hpack_dynamic_table_insert_at_start(HpackHeaderDynamicTable* const dynamic_table,
                                                   HpackHeaderDynamicEntry entry) {

	if(dynamic_table->count >= dynamic_table->capacity) {

		const size_t new_capacity = IMPL_CALC_GROW(dynamic_table->capacity);

		HpackHeaderDynamicEntry* new_entries = (HpackHeaderDynamicEntry*)realloc(
		    (void*)dynamic_table->entries, new_capacity * sizeof(HpackHeaderDynamicEntry));
		if(new_entries == NULL) {
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
#if DYNAMIC_HPACK_TABLE_FORTIFIED == 1
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

#if DYNAMIC_HPACK_TABLE_FORTIFIED == 1
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
