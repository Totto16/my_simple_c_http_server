

#pragma once

#include <json/json.h>

#include <ostream>
#include <ranges>

[[nodiscard]] bool operator==(const JsonVariant& json_variant1, const JsonVariant& json_variant2);

std::ostream& operator<<(std::ostream& os, const JsonVariant& json_variant);

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

	[[nodiscard]] JsonVariant& operator[](size_t index);

	[[nodiscard]] const JsonVariant& operator[](size_t index) const;

	[[nodiscard]] size_t size() const;

	[[nodiscard]] JsonVariant* begin();
	[[nodiscard]] JsonVariant* end();

	[[nodiscard]] const JsonVariant* begin() const;

	[[nodiscard]] const JsonVariant* end() const;
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

struct JsonVariantCpp {

	[[nodiscard]] static JsonVariant null() { return new_json_variant_null(); }

	[[nodiscard]] static JsonVariant boolean(const bool& value) {
		return new_json_variant_boolean(JsonBoolean{ .value = value });
	}

	[[nodiscard]] static JsonVariant number(const double& value) {
		return new_json_variant_number(JsonNumber{ .value = value });
	}

	[[nodiscard]] static JsonVariant number(const int64_t& value) {
		return number(static_cast<double>(value));
	}
};
