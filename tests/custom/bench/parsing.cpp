
#include <benchmark/benchmark.h>

#include <tmap.h>
#include <tvec.h>

#include <http/compression.h>
#include <http/header.h>
#include <http/parser.h>
#include <http/protocol.h>

#include <string>

#include <support/helpers.hpp>
#include <support/helpers/http.hpp>

static void BM_encoding_parser(benchmark::State& state) {

	assert(is_compression_supported(CompressionTypeGzip));
	assert(is_compression_supported(CompressionTypeDeflate));
	assert(is_compression_supported(CompressionTypeBr));
	assert(is_compression_supported(CompressionTypeZstd));
	assert(is_compression_supported(CompressionTypeCompress));

	assert(!is_compression_supported((CompressionType)(CompressionTypeCompress + 2)));

	for(auto _ : state) {

		// SUBCASE("no Accept-Encoding header")
		{

			HttpHeaderFields http_header_fields = TVEC_EMPTY(HttpHeaderField);

			compression::CompressionSettingsCpp compression_settings =
			    compression::CompressionSettingsCpp(http_header_fields);

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			assert(entries_length == 0);
		}

		// SUBCASE("standard simple list")
		{

			compression::CompressionSettingsCpp compression_settings =
			    compression::CompressionSettingsCpp(" compress, gzip");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			assert(entries_length == 2);

			CompressionEntry entry1 = TVEC_AT(CompressionEntry, compression_settings.entries(), 0);

			CompressionEntry entry1Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeCompress } },
				.weight = 1.0F
			};

			assert(entry1 == entry1Expected);

			CompressionEntry entry2 = TVEC_AT(CompressionEntry, compression_settings.entries(), 1);

			CompressionEntry entry2Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeGzip } },
				.weight = 1.0F
			};

			assert(entry2 == entry2Expected);
		}

		// SUBCASE("empty value")
		{

			compression::CompressionSettingsCpp compression_settings =
			    compression::CompressionSettingsCpp("");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			assert(entries_length == 0);
		}

		// SUBCASE("'*' value")
		{

			compression::CompressionSettingsCpp compression_settings =
			    compression::CompressionSettingsCpp(" *");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			assert(entries_length == 1);

			CompressionEntry entry1 = TVEC_AT(CompressionEntry, compression_settings.entries(), 0);

			CompressionEntry entry1Expected = {
				.value = { .type = CompressionValueTypeAllEncodings, .data = {} }, .weight = 1.0F
			};

			assert(entry1 == entry1Expected);
		}

		// SUBCASE("complicated list with weights")
		{
			compression::CompressionSettingsCpp compression_settings =
			    compression::CompressionSettingsCpp(" deflate;q=0.5, br;q=1.0");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			assert(entries_length == 2);

			CompressionEntry entry1 = TVEC_AT(CompressionEntry, compression_settings.entries(), 0);

			CompressionEntry entry1Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeDeflate } },
				.weight = 0.5F
			};

			assert(entry1 == entry1Expected);

			CompressionEntry entry2 = TVEC_AT(CompressionEntry, compression_settings.entries(), 1);

			CompressionEntry entry2Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeBr } },
				.weight = 1.0F
			};

			assert(entry2 == entry2Expected);
		}

		// SUBCASE("complicated list with weights and 'identity'")
		{
			compression::CompressionSettingsCpp compression_settings =
			    compression::CompressionSettingsCpp(" zstd;q=1.0, identity; q=0.5, *;q=0");

			size_t entries_length = TVEC_LENGTH(CompressionEntry, compression_settings.entries());

			assert(entries_length == 3);

			CompressionEntry entry1 = TVEC_AT(CompressionEntry, compression_settings.entries(), 0);

			CompressionEntry entry1Expected = {
				.value = { .type = CompressionValueTypeNormalEncoding,
				           .data = { .normal_compression = CompressionTypeZstd } },
				.weight = 1.0F
			};

			assert(entry1 == entry1Expected);

			CompressionEntry entry2 = TVEC_AT(CompressionEntry, compression_settings.entries(), 1);

			CompressionEntry entry2Expected = {
				.value = { .type = CompressionValueTypeNoEncoding, .data = {} }, .weight = 0.5F
			};

			assert(entry2 == entry2Expected);

			CompressionEntry entry3 = TVEC_AT(CompressionEntry, compression_settings.entries(), 2);

			CompressionEntry entry3Expected = {
				.value = { .type = CompressionValueTypeAllEncodings, .data = {} }, .weight = 0.0F
			};

			assert(entry3 == entry3Expected);
		}
	}
}

static void BM_url_parser(benchmark::State& state) {

	for(auto _ : state) {

		// SUBCASE("simple url")
		{

			auto parsed_path = http::ParsedURIWrapper::parse("/");

			assert(tstr_static_is_null(parsed_path.error()));

			const auto& path = parsed_path.path();

			const auto path_comp = string_from_tstr(path.path);

			assert(path_comp == "/");

			assert(TMAP_SIZE(ParsedSearchPathHashMap, &path.search_path.hash_map) == 0);
		}

		//	SUBCASE("real path url")
		{

			auto parsed_path = http::ParsedURIWrapper::parse("/test/hello");

			assert(tstr_static_is_null(parsed_path.error()));

			const auto& path = parsed_path.path();

			const auto path_comp = string_from_tstr(path.path);

			assert(path_comp == "/test/hello");

			assert(TMAP_SIZE(ParsedSearchPathHashMap, &path.search_path.hash_map) == 0);
		}

		// SUBCASE("path url with search parameters")
		{

			auto parsed_path =
			    http::ParsedURIWrapper::parse("/test/hello?param1=hello&param2&param3=");

			assert(tstr_static_is_null(parsed_path.error()));

			const auto& path = parsed_path.path();

			const auto path_comp = string_from_tstr(path.path);

			assert(path_comp == "/test/hello");

			const ParsedSearchPath search_path = path.search_path;

			assert(TMAP_SIZE(ParsedSearchPathHashMap, &search_path.hash_map) == 3);

			{

				const ParsedSearchPathEntry* entry =
				    find_search_key(search_path, "param1"_tstr_static);

				assert(entry != nullptr);

				assert(string_from_tstr(entry->key) == "param1");

				assert(string_from_tstr(entry->value.val) == "hello");
			}

			{

				const ParsedSearchPathEntry* entry =
				    find_search_key(search_path, "param2"_tstr_static);

				assert(entry != nullptr);

				assert(string_from_tstr(entry->key) == "param2");

				assert(string_from_tstr(entry->value.val) == "");
			}

			{
				const ParsedSearchPathEntry* entry =
				    find_search_key(search_path, "param3"_tstr_static);

				assert(entry != nullptr);

				assert(string_from_tstr(entry->key) == "param3");

				assert(string_from_tstr(entry->value.val) == "");
			}

			{

				const ParsedSearchPathEntry* entry =
				    find_search_key(search_path, "param4"_tstr_static);

				assert(entry == nullptr);
			}
		}

		// SUBCASE("path url with search parameters")
		{

			auto parsed_path =
			    http::ParsedURIWrapper::parse("/test/hello?param1=hello&param2&param3=");

			assert(tstr_static_is_null(parsed_path.error()));

			const auto& path = parsed_path.path();

			const auto path_comp = string_from_tstr(path.path);

			assert(path_comp == "/test/hello");

			const ParsedSearchPath search_path = path.search_path;

			assert(TMAP_SIZE(ParsedSearchPathHashMap, &search_path.hash_map) == 3);

			{

				const ParsedSearchPathEntry* entry =
				    find_search_key(search_path, "param1"_tstr_static);

				assert(entry != nullptr);

				assert(string_from_tstr(entry->key) == "param1");

				assert(string_from_tstr(entry->value.val) == "hello");
			}

			{

				const ParsedSearchPathEntry* entry =
				    find_search_key(search_path, "param2"_tstr_static);

				assert(entry != nullptr);

				assert(string_from_tstr(entry->key) == "param2");

				assert(string_from_tstr(entry->value.val) == "");
			}

			{
				const ParsedSearchPathEntry* entry =
				    find_search_key(search_path, "param3"_tstr_static);

				assert(entry != nullptr);

				assert(string_from_tstr(entry->key) == "param3");

				assert(string_from_tstr(entry->value.val) == "");
			}

			{

				const ParsedSearchPathEntry* entry =
				    find_search_key(search_path, "param4"_tstr_static);

				assert(entry == nullptr);
			}
		}
	}
}

BENCHMARK(BM_encoding_parser)->Name("parse/encoding");

BENCHMARK(BM_url_parser)->Name("parse/url");
