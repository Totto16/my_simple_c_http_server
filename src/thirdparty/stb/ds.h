

#pragma once

#define STBDS_NO_SHORT_NAMES
#include "./stb_ds.h"

#define STBDS_ARRAY(TypeName) TypeName*

#define STBDS_ARRAY_EMPTY NULL

#define STBDS_HASH_MAP_TYPE(KeyType, ValueType, TypeName) \
	typedef struct { \
		KeyType key; \
		ValueType value; \
	} TypeName

#define STBDS_HASH_MAP(TypeName) TypeName*

#define STBDS_HASH_MAP_EMPTY NULL
