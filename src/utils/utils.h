
#pragma once

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// see https://clang.llvm.org/docs/AttributeReference.html#nullability-attributes

#if defined(__GNUC__) || defined(__clang__)
	#if defined(__clang__)
		#define NULLABLE _Nullable
		#define NON_NULLABLE _Nonnull
		#define NO_NULLABLE_INFO _Null_unspecified
	#else
		#define NULLABLE
		#define NON_NULLABLE
		#define NO_NULLABLE_INFO
	#endif

	#define OUT_PARAM(type) type* NON_NULLABLE
	#define DEFAULT_PARAM(type) type* NO_NULLABLE_INFO
#elif defined(_MSC_VER)
	#define NULLABLE
	#define NON_NULLABLE
	#define NO_NULLABLE_INFO

	#define OUT_PARAM(type) _Out_ type*
	#define DEFAULT_PARAM(type) type*

#else
	#define NULLABLE
	#define NON_NULLABLE
	#define NO_NULLABLE_INFO

	#define OUT_PARAM(type) type*
	#define DEFAULT_PARAM(type) type*
#endif

#if _SIMPLE_SERVER_COMPILE_WITH_NARROWED_ENUMS
	#define C_23_NARROW_ENUM_TO(x) : x
	#define C_23_ENUM_TYPE(x) x
#else
	#define C_23_NARROW_ENUM_TO(x)
	#define C_23_ENUM_TYPE(x) int
#endif

#if defined(__clang__)
    // see: https://clang.llvm.org/docs/AttributeReference.html#flag-enum
	#define ENUM_IS_MASK __attribute__((flag_enum))
#else
	#define ENUM_IS_MASK
#endif

#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202000) || __cplusplus
	#define NODISCARD [[nodiscard]]
	#define MAYBE_UNUSED [[maybe_unused]]
#else
    // see e.g. https://www.gnu.org/software/gnulib/manual/html_node/Attributes.html
	#define NODISCARD __attribute__((__warn_unused_result__))
	#define MAYBE_UNUSED __attribute__((__unused__))
#endif

#define UNUSED(v) ((void)(v))

// cool trick from here:
// https://stackoverflow.com/questions/777261/avoiding-unused-variables-warnings-when-using-assert-in-a-release-build
#ifdef NDEBUG
	#define assert(x) /* NOLINT(readability-identifier-naming) */ \
		do { \
			UNUSED((x)); \
		} while(false)
#else

	#include <assert.h>

#endif

#ifdef NDEBUG
	#define UNREACHABLE() \
		do { \
			fprintf(stderr, "[%s %s:%d]: UNREACHABLE", __func__, __FILE__, __LINE__); \
			exit(EXIT_FAILURE); \
		} while(false)
#else

	#define UNREACHABLE() \
		do { \
			assert(false && "UNREACHABLE"); \
		} while(false)

#endif

// simple error helper macro, with some more used "overloads"
#define CHECK_FOR_ERROR(toCheck, errorString, statement) \
	do { \
		if((toCheck) == -1) { \
			LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation), "%s: %s\n", \
			            errorString, strerror(errno)); \
			statement; \
		} \
	} while(false)

#define CHECK_FOR_THREAD_ERROR(toCheck, errorString, statement) \
	do { \
		if((toCheck) != 0) { \
			/*pthread function don't set errno, but return the error value \
			 * directly*/ \
			LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelError, LogPrintLocation), "%s: %s\n", \
			            errorString, strerror(toCheck)); \
			statement; \
		} \
	} while(false)

#define CHECK_RESULT_FOR_THREAD_ERROR(errorString, statement) \
	CHECK_FOR_THREAD_ERROR(result, errorString, statement)

// copied from exercises before (PS 1-7, selfmade), it safely parses a long!
NODISCARD long parse_long_safely(const char* to_parse, const char* description);

NODISCARD long parse_long(const char* to_parse, OUT_PARAM(bool) success);

NODISCARD size_t parse_size_t(const char* to_parse, OUT_PARAM(bool) success);

NODISCARD uint16_t parse_u16_safely(const char* to_parse, const char* description);

// a hacky but good and understandable way that is used with pthread functions
// to annotate which type the really represent
#define ANY void*

// Type helper for readability
#define ANY_TYPE(type) ANY

// uses snprintf feature with passing NULL,0 as first two arguments to automatically determine the
// required buffer size, for more read man page
// for variadic functions its easier to use macro
// magic, attention, use this function in the right way, you have to prepare a char* that is set to
// null, then it works best! snprintf is safer then sprintf, since it guarantees some things, also
// it has a failure indicator
#define FORMAT_STRING_IMPL(toStore, statement, logger_fn, format, ...) \
	{ \
		char* internalBuffer = *toStore; \
		if(internalBuffer != NULL) { \
			free(internalBuffer); \
		} \
		int toWrite = snprintf(NULL, 0, format, __VA_ARGS__) + 1; \
		internalBuffer = (char*)malloc(toWrite * sizeof(char)); \
		if(!internalBuffer) { \
			logger_fn("Couldn't allocate memory for %d bytes!\n", toWrite); \
			statement \
		} \
		int written = snprintf(internalBuffer, toWrite, format, __VA_ARGS__); \
		if(written >= toWrite) { \
			logger_fn("snprintf did write more bytes then it had space in the buffer, available " \
			          "space: '%d', actually written: '%d'!\n", \
			          (toWrite) - 1, written); \
			free(internalBuffer); \
			statement \
		} \
		*toStore = internalBuffer; \
	} \
	if(*toStore == NULL) { \
		logger_fn("snprintf Macro gone wrong: '%s' is pointing to NULL!\n", #toStore); \
		statement \
	}

#define FORMAT_STRING(toStore, statement, format, ...) \
	FORMAT_STRING_IMPL(toStore, statement, IMPL_LOGGER_DEFAULT, format, __VA_ARGS__)

#define IMPL_LOGGER_DEFAULT(format, ...) \
	LOG_MESSAGE(COMBINE_LOG_FLAGS(LogLevelWarn, LogPrintLocation), format, __VA_ARGS__)

#define IMPL_STDERR_LOGGER(format, ...) fprintf(stderr, format, __VA_ARGS__)

NODISCARD float parse_float(char* value);

NODISCARD uint32_t get_random_byte(void);

NODISCARD uint32_t get_random_byte_in_range(uint32_t min, uint32_t max);

NODISCARD int get_random_bytes(size_t size, OUT_PARAM(uint8_t) out_bytes);

#define TSTR_KEYNAME TString

#define STRINGIFY(a) STR_IMPL(a)
#define STR_IMPL(a) #a
