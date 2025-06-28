

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
int string_builder_append_string(StringBuilder* stringBuilder, char* string) {
	size_t length = strlen(string);
	// if te string builder is empty malloc the right size
	if(stringBuilder->currentSize == 0) {
		// +1, so one trailing 0 byte is there :)
		stringBuilder->data = (char*)malloc(length + 1);

		if(!stringBuilder->data) {
			LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
			return -1;
		}

		stringBuilder->data[length] = '\0';

		memcpy(stringBuilder->data, string, length);

	} else {
		// otherwise realloc, this realloc wrapper takes care of everything, then afterwards the
		// memcpy copies everything in the right place, leaving a trailing null character at the
		// end
		stringBuilder->data =
		    (char*)realloc(stringBuilder->data, stringBuilder->currentSize + length + 1);

		if(!stringBuilder->data) {
			LOG_MESSAGE_SIMPLE(LogLevelWarn | LogPrintLocation, "Couldn't allocate memory!\n");
			return -1;
		}

		stringBuilder->data[stringBuilder->currentSize + length] = '\0';

		memcpy(stringBuilder->data + stringBuilder->currentSize, string, length);
	}
	stringBuilder->currentSize += length; // trailing 0 byte is not included
	// then free the input, since the bytes are already in the stringbuilder
	free(string);

	return 0;
}

// simple wrapper if just a constant string has to be appended
int string_builder_append_single(StringBuilder* stringBuilder, const char* notMallocedString) {
	char* mallocedString = normalStringToMalloced(notMallocedString);
	return string_builder_append_string(stringBuilder, mallocedString);
}

// attention the two methods to_string and get_string are different in that sense, that after
// to_string the Stringbuilder is freed and invalid, after get_string not!

// the struct or implementation can change, this function has to adapt, not thew user!
// ATTENTION: after this call the stringbuilder is destroyed! meaning the string you receive is a
// single malloced string you have to take care of
char* string_builder_to_string(StringBuilder* stringBuilder) {
	char* result = stringBuilder->data;
	free(stringBuilder);
	return result;
}

// the struct or implementation can change, this function has to adapt, not the user!
// after that call the stringbuilder is reusable and can be freed, appended uppon etc.
char* string_builder_get_string(StringBuilder* stringBuilder) {
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
