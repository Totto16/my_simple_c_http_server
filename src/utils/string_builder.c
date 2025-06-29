

#include "string_builder.h"

#include "utils/log.h"

StringBuilder* string_builder_init() {
	StringBuilder* result = (StringBuilder*)malloc(sizeof(StringBuilder));
	if(!result) {
		return NULL;
	}

	result->currentSize = 0;
	result->data = NULL;

	return result;
}

// helper function that turns a normal string into a malloced one, so the lifetime is extended and
// he can be freed!

char* normalStringToMalloced(const char* notMallocedString) {
	size_t length = strlen(notMallocedString);
	char* mallocedString = (char*)malloc(length + 1);

	if(!mallocedString) {
		LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
		return NULL;
	}

	mallocedString[length] = '\0';

	memcpy(mallocedString, notMallocedString, length);
	return mallocedString;
}

// the actual append method, it accepts a string builder where to append and then appends the body
// string there
static int __string_builder_append_string_impl(StringBuilder* stringBuilder, char* string,
                                               size_t size) {
	// if te string builder is empty malloc the right size
	if(stringBuilder->currentSize == 0) {
		// +1, so one trailing 0 byte is there :)
		stringBuilder->data = (char*)malloc(size + 1);

		if(!stringBuilder->data) {
			LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
			return -1;
		}

		stringBuilder->data[size] = '\0';

		memcpy(stringBuilder->data, string, size);

	} else {
		// otherwise realloc, this realloc wrapper takes care of everything, then afterwards the
		// memcpy copies everything in the right place, leaving a trailing null character at the
		// end
		stringBuilder->data =
		    (char*)realloc(stringBuilder->data, stringBuilder->currentSize + size + 1);

		if(!stringBuilder->data) {
			LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
			return -1;
		}

		stringBuilder->data[stringBuilder->currentSize + size] = '\0';

		memcpy(stringBuilder->data + stringBuilder->currentSize, string, size);
	}
	stringBuilder->currentSize += size; // trailing 0 byte is not included
	// then free the input, since the bytes are already in the stringbuilder
	free(string);

	return 0;
}

int string_builder_append_string(StringBuilder* stringBuilder, char* string) {
	size_t length = strlen(string);

	return __string_builder_append_string_impl(stringBuilder, string, length);
}

// simple wrapper if just a constant string has to be appended
int string_builder_append_single(StringBuilder* stringBuilder, const char* notMallocedString) {
	char* mallocedString = normalStringToMalloced(notMallocedString);
	return string_builder_append_string(stringBuilder, mallocedString);
}

char* string_builder_to_string_deprecated(StringBuilder* stringBuilder) {
	char* result = stringBuilder->data;
	free(stringBuilder);
	return result;
}

int string_builder_append_string_builder(StringBuilder* stringBuilder,
                                         StringBuilder* stringBuilder2) {
	int result = __string_builder_append_string_impl(stringBuilder, stringBuilder2->data,
	                                                 stringBuilder2->currentSize);

	stringBuilder2->data = NULL;
	stringBuilder2->currentSize = 0;

	return result;
}

char* string_builder_get_string_deprecated(StringBuilder* stringBuilder) {
	return stringBuilder->data;
}

NODISCARD SizedBuffer string_builder_get_sized_buffer(StringBuilder* stringBuilder) {
	return (SizedBuffer){ .data = stringBuilder->data, .size = stringBuilder->currentSize };
}

// just free the stringbuilder and the associated string
void free_string_builder(StringBuilder* stringBuilder) {
	free(stringBuilder->data);
	free(stringBuilder);
}
