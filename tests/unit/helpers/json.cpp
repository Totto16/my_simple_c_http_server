#include "./json.hpp"

#include <fstream>

[[nodiscard]] static std::vector<std::pair<std::string, std::string>>
parse_headers_map(const nlohmann::json& value) {

	std::vector<std::pair<std::string, std::string>> result{};

	if(!value.is_array()) {
		throw std::runtime_error("json is malformed");
	}

	for(const auto& val : value) {

		if(!val.is_object()) {
			throw std::runtime_error("json is malformed");
		}

		for(auto& el : val.items()) {
			result.emplace_back(el.key(), el.value());
		}
	}

	return result;
}

[[nodiscard]] static std::optional<size_t> get_optional_header_size(const nlohmann::json& case_) {

	if(!case_.contains("header_table_size")) {
		return std::nullopt;
	}

	const auto& header_table_size_v = case_["header_table_size"];

	if(header_table_size_v.is_null()) {
		return std::nullopt;
	}

	return header_table_size_v.get<size_t>();
}

[[nodiscard]] static tests::ThirdPartyHpackTestCaseEntry
get_case_from_json(const nlohmann::json& value, const std::string& suite_name,
                   const std::string& test_name,
                   const std::vector<consts::StrictErrorException>& strict_error_state_exceptions) {

	size_t seqno = value["seqno"].get<size_t>();

	const std::string raw_wire_data = value["wire"].get<std::string>();

	const auto wire_data = hpack::helpers::parse_wire_data(raw_wire_data);

	const auto headers = parse_headers_map(value["headers"]);

	const auto header_table_size = get_optional_header_size(value);

	std::vector<std::string> strict_error_state{};

	for(const auto& header : headers) {
		if(header.second == "") {

			// some exceptions, as they aree either encoded as 0 byte string and not using an
			// invalid entry value (NULL != "")
			if(vec_contains(strict_error_state_exceptions, consts::StrictErrorException{
			                                                   .suite_name = suite_name,
			                                                   .test_name = test_name,
			                                                   .seqno = seqno,
			                                                   .field_name = header.first,
			                                               })) {
				continue;
			}

			strict_error_state.emplace_back(header.first);
		}
	}

	return tests::ThirdPartyHpackTestCaseEntry{
		.seqno = seqno,
		.wire_data = wire_data,
		.headers = headers,
		.header_table_size = header_table_size.value_or(consts::default_header_table_size),
		.strict_error_state = strict_error_state,
	};
}

[[nodiscard]] static tests::ThirdPartyHpackTestCase get_thirdparty_hpack_test_case(
    const std::filesystem::path& path,
    const std::vector<consts::StrictErrorException>& strict_error_state_exceptions) {

	std::ifstream file_stream{ path };
	nlohmann::json data = nlohmann::json::parse(file_stream);

	const std::string description = data.at("description").get<std::string>();

	const std::string test_name = path.filename().stem();

	const std::string suite_name = path.parent_path().filename().string();

	std::vector<tests::ThirdPartyHpackTestCaseEntry> cases{};

	if(!data.contains("cases") || !data["cases"].is_array()) {
		throw std::runtime_error("json is malformed");
	}

	for(size_t i = 0; i < data["cases"].size(); ++i) {
		const auto& case_ = data["cases"].at(i);

		const auto case_result =
		    get_case_from_json(case_, suite_name, test_name, strict_error_state_exceptions);

		cases.push_back(case_result);
	}

	// cases post processing
	tests::HeaderTableMode header_mode = { .all_the_same = true,
		                                   .table_size = consts::default_header_table_size };

	{
		std::optional<size_t> header_table_size = std::nullopt;

		for(size_t i = 0; i < cases.size(); ++i) {
			const auto& case_ = cases.at(i);

			const auto local_h_size = case_.header_table_size;

			if(local_h_size.has_value()) {
				if(header_table_size.has_value()) {
					if(local_h_size.value() != header_table_size.value()) {

						header_mode.all_the_same = false;
						break;
					}
				} else {
					header_table_size = local_h_size;
				}
			}
		}

		if(header_mode.all_the_same) {
			header_mode.table_size = header_table_size.value_or(consts::default_header_table_size);
		} else {
			header_mode.table_size =
			    cases.size() == 0
			        ? consts::default_header_table_size
			        : cases.at(0).header_table_size.value_or(consts::default_header_table_size);
		}
	}

	return tests::ThirdPartyHpackTestCase{
		.description = description,
		.cases = cases,
		.test_name = test_name,
		.header_mode = header_mode,
		.file = path,
	};
}

[[nodiscard]] static std::filesystem::path get_root_test_dir() {
	const std::filesystem::path root_tests_dir1 =
	    std::filesystem::current_path() / ".." / "thirdparty" / "hpack-test-case";

	if(std::filesystem::exists(root_tests_dir1)) {
		return root_tests_dir1;
	}

	const std::filesystem::path root_tests_dir2 =
	    std::filesystem::current_path() / "tests" / "unit" / "thirdparty" / "hpack-test-case";

	if(std::filesystem::exists(root_tests_dir2)) {
		return root_tests_dir2;
	}

	return root_tests_dir2;
}

[[nodiscard]] std::vector<tests::ThirdPartyHpackTestCase>
hpack::get_thirdparty_hpack_test_cases(const std::string& name) {

	const std::filesystem::path root_tests_dir = get_root_test_dir();

	if(!std::filesystem::exists(root_tests_dir)) {
		throw std::runtime_error("Invalid test launch from invalid cwd!");
	}

	const std::filesystem::path dir = (root_tests_dir / name).lexically_normal();

	if(!std::filesystem::exists(dir)) {
		throw std::runtime_error(std::string{ "Invalid test dir name: " } + dir.string());
	}

	if(!std::filesystem::is_directory(dir)) {
		throw std::runtime_error(std::string{ "Invalid test dir name: (no a dir)" } + dir.string());
	}

	std::vector<tests::ThirdPartyHpackTestCase> result{};

	const auto& strict_error_state_exceptions = ::helpers::get_strict_error_state_exceptions();

	if(vec_contains_duplicate(strict_error_state_exceptions)) {
		throw std::runtime_error(
		    "duplicate in strict_error_state_exceptions vector found, this is not allowed!");
	}

	for(auto const& dir_entry : std::filesystem::directory_iterator{ dir }) {

		if(!dir_entry.is_regular_file()) {
			continue;
		}

		const auto value =
		    get_thirdparty_hpack_test_case(dir_entry.path(), strict_error_state_exceptions);

		result.push_back(value);
	}

	return result;
}
