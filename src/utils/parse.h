

#pragma once

#include "./sized_buffer.h"

typedef struct {
	size_t cursor;
	SizedBuffer data;
} ParseState;

typedef uint8_t Byte;

/**
 * @brief Get the until delimiter fixed object
 *
 * @param state
 * @param delimiter
 * @return Byte* - is NULL when the delimter was not found, if it is a pointer, it points into or
 * after the state.data, it point to the first character of the delimiter alias the end of the valid
 * range
 */
NODISCARD Byte* parser_get_until_delimiter(ParseState* state, const char* delimiter);

/**
 * @brief Get the until delimiter fixed object
 *
 * @param state
 * @param delimiter
 * @return Byte* - is NULL when the delimter was not found, if it is a pointer, it points into or
 * after the state.data, it point to the first character of the delimiter alias the end of the valid
 * range
 */
NODISCARD Byte* parser_get_until_delimiter_fixed(ParseState* state, SizedBuffer delimiter);
