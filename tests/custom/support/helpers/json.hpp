

#pragma once

#include <json/json.h>

#include <ostream>
#include <ranges>

[[nodiscard]] bool operator==(const JsonValue& json_value1, const JsonValue& json_value2);

std::ostream& operator<<(std::ostream& os, const JsonValue& json_value);

[[nodiscard]] bool operator==(const JsonBoolean& json_boolean1, const JsonBoolean& json_boolean2);

[[nodiscard]] bool operator==(const JsonNumber& json_number1, const JsonNumber& json_number2);

struct JsonStringCpp {
  private:
	JsonString* m_value;

  public:
	explicit JsonStringCpp(JsonString* value);

	[[nodiscard]] bool operator==(const JsonStringCpp& json_string2) const;

	[[nodiscard]] bool operator==(const JsonString* json_string2) const;
};

struct JsonArrayCpp {
  private:
	JsonArray* m_value;

  public:
	explicit JsonArrayCpp(JsonArray* value);

	[[nodiscard]] bool operator==(const JsonArrayCpp& json_array2) const;

	[[nodiscard]] bool operator==(const JsonArray* json_array2) const;

	[[nodiscard]] JsonValue& operator[](size_t index);

	[[nodiscard]] const JsonValue& operator[](size_t index) const;

	[[nodiscard]] size_t size() const;

	[[nodiscard]] JsonValue* begin();
	[[nodiscard]] JsonValue* end();

	[[nodiscard]] const JsonValue* begin() const;

	[[nodiscard]] const JsonValue* end() const;
};

static_assert(std::ranges::range<JsonArrayCpp>);

struct JsonObjectEntryCpp {
	int todo;
};

struct JsonObjectCpp {
  private:
	JsonObject* m_value;

  public:
	explicit JsonObjectCpp(JsonObject* value);

	[[nodiscard]] bool operator==(const JsonObjectCpp& json_object2) const;

	[[nodiscard]] bool operator==(const JsonObject* json_object2) const;

	[[nodiscard]] const JsonObjectEntryCpp* begin() const;

	[[nodiscard]] const JsonObjectEntryCpp* end() const;
};

static_assert(std::ranges::range<JsonObjectCpp>);

struct JsonValueCpp {

	[[nodiscard]] static JsonValue null();

	[[nodiscard]] static JsonValue boolean(const bool& value);

	[[nodiscard]] static JsonValue number(const double& value);

	[[nodiscard]] static JsonValue number(const int64_t& value);

	[[nodiscard]] static JsonValue string(const std::string& value);

	[[nodiscard]] static JsonValue array(std::initializer_list<JsonValue>&& values);

	[[nodiscard]] static JsonValue
	object(std::initializer_list<std::pair<std::string, JsonValue>>&& values);
};

struct JsonErrorCpp {
  private:
	std::string m_message;
	SourceLocation m_loc;

	JsonErrorCpp(std::string&& message, SourceLocation loc);

  public:
	explicit JsonErrorCpp(const JsonError& value);

	static JsonErrorCpp with_no_loc(std::string&& value);

	static JsonErrorCpp with_string_loc(std::string&& value, tstr_view data, SourcePosition pos);

	[[nodiscard]] bool operator==(const JsonErrorCpp& json_error2) const;

	[[nodiscard]] bool operator==(const JsonError& json_error2) const;

	friend std::ostream& operator<<(std::ostream& os, const JsonErrorCpp& json_error);
};

std::ostream& operator<<(std::ostream& os, const JsonErrorCpp& json_error);

std::ostream& operator<<(std::ostream& os, const JsonError& json_error);
