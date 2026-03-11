#include <benchmark/benchmark.h>

#include <generic/hash.h>

#include <array>
#include <cstdint>
#include <vector>

NODISCARD static bool operator==(const SizedBuffer& lhs, const SizedBuffer& rhs) {

	if(lhs.size != rhs.size) {
		return false;
	}

	if(lhs.data == NULL && rhs.data == NULL) {
		return true;
	}

	if(lhs.data == NULL || rhs.data == NULL) {
		return false;
	}

	auto* lhs_ptr = static_cast<std::uint8_t*>(lhs.data);
	auto* rhs_ptr = static_cast<std::uint8_t*>(rhs.data);

	for(size_t i = 0; i < lhs.size; ++i) {
		if(lhs_ptr[i] != rhs_ptr[i]) {
			return false;
		}
	}

	return true;
}

namespace {

[[nodiscard]] consteval std::uint8_t single_hex_number(char input, OUT_PARAM(bool) success) {
	if(input >= '0' && input <= '9') {
		*success = true;
		return static_cast<std::uint8_t>(input - '0');
	}

	if(input >= 'A' && input <= 'F') {
		*success = true;
		return static_cast<std::uint8_t>(input - 'A' + 10);
	}

	if(input >= 'a' && input <= 'f') {
		*success = true;
		return static_cast<std::uint8_t>(input - 'a' + 10);
	}

	*success = false;
	return 0;
	;
}

[[nodiscard]] consteval std::uint8_t single_hex_value(const char* input, OUT_PARAM(bool) success) {

	const auto first = single_hex_number(input[0], success);

	if(!(*success)) {
		return 0;
	}

	const auto second = single_hex_number(input[1], success);

	if(!(*success)) {
		return 0;
	}

	*success = true;
	return (first << 4) | second;
}

constexpr const size_t sha1_buffer_size = 20;

struct Sha1BufferType {
  public:
	using ValueType = std::uint8_t;
	using UnderlyingType = std::array<ValueType, sha1_buffer_size>;

  private:
	UnderlyingType m_value;
	bool m_is_error;

  public:
	constexpr Sha1BufferType(UnderlyingType&& value)
	    : m_value{ std::move(value) }, m_is_error{ false } {}

	constexpr Sha1BufferType(bool is_error) : m_value{}, m_is_error{ is_error } {}

	constexpr ValueType& operator[](UnderlyingType::size_type n) noexcept { return m_value[n]; }

	[[nodiscard]] constexpr bool is_error() const { return m_is_error; }

	constexpr void set_error(bool error) { m_is_error = error; }

	[[nodiscard]] constexpr SizedBuffer get_sized_buffer() const {
		SizedBuffer sized_buffer = { .data = (void*)&this->m_value, .size = sha1_buffer_size };
		return sized_buffer;
	}

	[[nodiscard]] bool operator==(const SizedBuffer& lhs) const {

		SizedBuffer rhs_sized_buffer = this->get_sized_buffer();

		return lhs == rhs_sized_buffer;
	}
};

[[nodiscard]] consteval Sha1BufferType get_expected_sha1_from_string(const char* input,
                                                                     std::size_t size) {

	Sha1BufferType result = { false };

	if(size == 0) {
		return result;
	}

	if(size % 2 != 0) {
		return result;
	}

	size_t buffer_size = size / 2;

	if(buffer_size != sha1_buffer_size) {
		return result;
	}

	for(size_t i = 0; i < buffer_size; ++i) {
		bool success_sub = true;
		std::uint8_t value = single_hex_value(input + (i * 2), &success_sub);
		if(!success_sub) {
			return result;
		}
		result[i] = value;
	}

	result.set_error(false);
	return result;
}

[[nodiscard]] consteval Sha1BufferType operator""_sha1(const char* input, std::size_t size) {
	const auto result = get_expected_sha1_from_string(input, size);

	if(result.is_error()) {
		assert(false && "ERROR in consteval");
	}

	return result;
}

struct TestCaseSha1 {
	std::string name;
	std::string input;
	Sha1BufferType result;
};

struct TestCaseBase64 {
	std::string name;
	std::string raw;
	std::string base64;
};

} // namespace

static void BM_sha1(benchmark::State& state) {

	std::string sha1_provider = get_sha1_provider();
	assert(sha1_provider == "openssl (EVP)");

	std::vector<TestCaseSha1> test_cases = {
		{ .name = "empty string",
		  .input = "",
		  .result = "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709"_sha1 },
		{ .name = "simple string",
		  .input = "hello world",
		  .result = "2AAE6C35C94FCFB415DBE95F408B9CE91EE846ED"_sha1 },
		{ .name = "longer string",
		  .input = R"(Lorem ipsum dolor sit amet,
		  consectetur adipiscing elit.Sed sit amet scelerisque
		      tortor.Nullam eget est eu sapien molestie tincidunt.Aenean urna mi,
		  gravida ac varius egestas,
		  bibendum non magna.Nullam luctus metus at fringilla efficitur.Suspendisse dapibus,
		  risus eu varius auctor,
		  mauris sem lacinia elit,
		  vel euismod est est eu urna.Duis et condimentum sem.Morbi laoreet porta felis in
		      tempus.Quisque mattis ex sit amet metus venenatis,
		  ut luctus mi commodo.Fusce rutrum libero a lacus porttitor,
		  id laoreet justo efficitur.Class aptent taciti sociosqu ad litora torquent per conubia
		      nostra,
		  per inceptos himenaeos.Nunc magna arcu,
		  pellentesque non aliquet sit amet,
		  egestas id massa.Vivamus scelerisque,
		  elit in rhoncus interdum,
		  felis nisi porta magna,
		  nec blandit nunc urna a diam.

		  Duis tellus erat,
		  finibus ut placerat vitae,
		  tristique non lectus.Pellentesque mattis nulla ut est luctus mollis.Nam tincidunt eros
		      mauris,
		  ac sodales eros accumsan ac.Suspendisse rhoncus felis id sapien aliquam interdum.Ut ex
		      nunc,
		  fringilla sed euismod ut,
		  mattis cursus quam.Nullam vel eros
		      nisl.Nulla facilisi.Aenean scelerisque eget massa id
		          molestie.Integer sit amet urna tortor.Fusce libero massa,
		  rhoncus nec ultrices non,
		  sollicitudin vel erat.Maecenas gravida,
		  purus vitae consectetur cursus,
		  sem nibh tincidunt risus,
		  in cursus lectus sapien a diam.Aenean at est et nunc feugiat
		      consequat.Aliquam pellentesque eu dui eu faucibus.Vivamus aliquam efficitur
		          congue.Donec venenatis odio vulputate malesuada accumsan.

		  Vivamus felis erat,
		  dapibus id auctor sed,
		  feugiat vitae augue.Nam eget felis magna.Maecenas aliquet dui in nulla aliquet
		      euismod.Cras egestas cursus dolor,
		  ut molestie nisl auctor at.Aenean ut sodales tortor,
		  porta ultrices nunc.Duis fermentum velit at enim feugiat,
		  id mattis felis sagittis.Mauris iaculis,
		  mauris laoreet ultrices cursus,
		  mi nunc consectetur turpis,
		  eu porta justo nibh at risus
		      .

		  Fusce sollicitudin ante eget quam commodo posuere convallis at mi.Quisque fermentum augue
		      vel tortor consectetur,
		  sed mattis sapien molestie.Donec fermentum eros sed elementum auctor.Proin id sodales
		      odio,
		  vel bibendum nunc.Nam in egestas diam.Cras bibendum est orci,
		  interdum faucibus justo convallis eget.Cras non risus ac nulla pretium sagittis.

		  Etiam lorem nunc,
		  pulvinar ornare convallis ac,
		  gravida eu velit.Aenean dapibus cursus facilisis.Aliquam ut nibh nulla
		      .In hac habitasse platea dictumst.Cras ultricies turpis et tellus posuere
		          ullamcorper.Nunc sodales arcu ut erat vestibulum,
		  ac elementum lectus eleifend.Sed rutrum tortor quis dolor mollis
		      convallis.Fusce sollicitudin orci leo,
		  sed interdum libero imperdiet id.

		  Vestibulum quam augue,
		  iaculis vitae viverra a,
		  auctor vitae est.Pellentesque nisi erat,
		  ultricies auctor porttitor tincidunt,
		  porta eu turpis.Nam gravida commodo
		      metus.Phasellus lacinia felis sed nunc eleifend
		          malesuada.Vivamus malesuada tortor ut gravida bibendum.Quisque maximus mauris eget
		              dolor tempor,
		  in semper lacus elementum.Ut non fringilla justo,
		  ut molestie lacus.Curabitur blandit enim urna.Curabitur turpis turpis,
		  pretium et ex quis,
		  pellentesque semper lorem.Vivamus finibus ac urna quis
		      ultricies.Nullam dignissim volutpat blandit.Sed accumsan efficitur lectus,
		  vitae molestie justo condimentum
		      sed.Praesent et lorem nec nisi interdum fringilla vitae et
		          quam.

		  Phasellus convallis neque et leo consectetur
		      pulvinar.Sed ac diam et ipsum sodales efficitur.Curabitur lectus justo,
		  rutrum vel massa a,
		  porta tincidunt tortor.Duis congue leo ultricies enim lacinia,
		  quis tempus mi gravida.Mauris porttitor metus iaculis rutrum sodales.Nunc in scelerisque
		      diam,
		  vitae blandit leo.Nullam a dignissim massa,
		  sed mattis eros.Vivamus leo leo,
		  mattis id est vitae,
		  sagittis convallis sapien.Integer pulvinar tortor in orci luctus mattis.Integer convallis
		      viverra nibh,
		  eget viverra massa.Fusce nec mollis nulla.Sed et tempor elit,
		  a consectetur mi.Vivamus tortor enim,
		  molestie non venenatis at,
		  tristique eget sem.Nunc sit amet purus pharetra,
		  ultricies metus quis,
		  gravida mi
		      .

		  Nullam interdum ornare viverra.Duis in condimentum
		      mi.Sed rutrum massa sit amet quam hendrerit laoreet.Curabitur est tellus,
		  consectetur non blandit a,
		  lobortis vitae augue.Sed luctus lacus elit,
		  vel condimentum metus accumsan
		      quis.Proin laoreet diam vel elit condimentum iaculis vel quis
		          quam.Pellentesque luctus nulla pretium elit accumsan imperdiet.Donec iaculis,
		  odio id vestibulum dictum,
		  ante justo tincidunt leo,
		  eget mattis justo neque vitae justo.Duis lacus neque,
		  tincidunt sit amet turpis vel,
		  elementum facilisis
		      elit.

		  Etiam mollis ut nunc sed bibendum.Proin ultrices enim nulla,
		  ac faucibus nunc elementum ac.Fusce maximus tempor mi ut
		      rhoncus.Aliquam at augue non ipsum lobortis congue.Ut et egestas velit,
		  quis mattis sapien.Nunc auctor purus id tempus volutpat.Nam nulla velit,
		  pharetra vitae urna a,
		  rhoncus blandit urna.Morbi vulputate ullamcorper velit sit amet ornare.Morbi ut
		      condimentum arcu,
		  sodales egestas justo.Integer molestie arcu elit,
		  non vestibulum nibh mollis in.Ut id ex vel justo molestie placerat.Cras dapibus posuere
		      ex,
		  vehicula tincidunt tortor consequat sed.In est ligula,
		  consectetur non tempus vitae,
		  ultrices porta urna.Quisque quam arcu,
		  sollicitudin sit amet arcu a,
		  rutrum malesuada nulla.Maecenas gravida tellus nibh,
		  at feugiat justo faucibus in.Fusce id imperdiet odio,
		  ut viverra
		      erat.

		  Aliquam erat volutpat.Quisque ultrices lobortis erat pulvinar
		      lobortis.Curabitur in nisl dui.Vestibulum scelerisque blandit nisi non
		          mattis.Aenean massa ante,
		  vulputate at accumsan nec,
		  suscipit id neque.Cras vitae sagittis nunc.Duis semper,
		  mauris a condimentum mollis,
		  augue turpis dapibus lectus,
		  ut pulvinar sapien justo ut leo.Vivamus scelerisque arcu sed finibus
		      ornare.Interdum et malesuada fames ac ante ipsum primis in faucibus.Cras aliquam
		          luctus turpis,
		  non tempor tortor sollicitudin ac.Maecenas id ligula luctus,
		  ultricies velit et,
		  facilisis ante.Sed in faucibus mauris,
		  a luctus mauris.Fusce feugiat est a lobortis sagittis.Duis placerat risus turpis,
		  faucibus dignissim purus sagittis
		      eget.

		  Aenean ac eros at nunc accumsan aliquet vitae et eros.Donec sit amet ipsum scelerisque,
		  aliquam libero quis,
		  semper nisl.Cras sed congue metus.Curabitur consectetur rhoncus quam,
		  in imperdiet risus tristique sed.Class aptent taciti sociosqu ad litora torquent per
		      conubia nostra,
		  per inceptos himenaeos.Etiam sed enim ac nunc cursus luctus.Lorem ipsum dolor sit amet,
		  consectetur adipiscing elit.Suspendisse potenti.Nulla erat magna,
		  accumsan sed nulla feugiat,
		  ultrices viverra ipsum.Proin at efficitur urna,
		  eu rhoncus enim.Aenean ac felis magna.Etiam massa magna,
		  faucibus quis dolor at,
		  gravida dapibus orci.Aliquam sodales rutrum tortor,
		  ultrices auctor augue dictum
		      nec.

		  Quisque porttitor porttitor rutrum.Integer in nunc sagittis,
		  eleifend velit at,
		  varius magna.Ut et porta sapien,
		  ac euismod diam.In luctus,
		  lacus et porttitor venenatis,
		  sapien tortor gravida velit,
		  eu finibus ligula nulla ut
		      leo.Interdum et malesuada fames ac ante ipsum primis in
		          faucibus.Nam elementum at libero ut
		              aliquam.Proin auctor porttitor lorem vitae euismod.Proin vel tempor est,
		  eget maximus tellus.Suspendisse lacus ligula,
		  elementum eu velit et,
		  bibendum porttitor sapien.Aliquam purus purus,
		  condimentum et fermentum eu,
		  suscipit eget velit.In convallis pretium ex et maximus.Maecenas rutrum molestie ante,
		  id consequat
		      .)",
		  .result = "C46101B79C6967878813B346456145B64DF22DED"_sha1 }

	};

	for(auto _ : state) {
		// This code gets timed

		for(const auto& test_case : test_cases) {

			const SizedBuffer result = get_sha1_from_string(test_case.input.c_str());

			assert(result.data != nullptr);
			assert(result.size != 0);

			assert(result == test_case.result);

			free_sized_buffer(result);
		}
	}
}

namespace {
std::vector<TestCaseBase64> base64_test_cases = {
	{ .name = "empty string", .raw = "", .base64 = "" },
	{ .name = "simple string", .raw = "hello world", .base64 = "aGVsbG8gd29ybGQ=" },
	{ .name = "longer string",
	  .raw = R"(😎😎😎😎😎😎🌍Hello test long string)",
	  .base64 = "8J+YjvCfmI7wn5iO8J+YjvCfmI7wn5iO8J+MjUhlbGxvIHRlc3QgbG9uZyBzdHJpbmc=" }

};

[[nodiscard]] SizedBuffer buffer_from_string(const std::string& inp) {
	return { .data = (void*)inp.c_str(), .size = inp.size() };
}

} // namespace

static void BM_base64_dec(benchmark::State& state) {

	std::string base64_provider = get_base64_provider();
	assert(base64_provider == "openssl");

	for(auto _ : state) {
		// This code gets timed

		for(const auto& test_case : base64_test_cases) {

			SizedBuffer input = buffer_from_string(test_case.base64);

			const SizedBuffer result = base64_decode_buffer(input);

			assert(result.data != nullptr);

			if(input.size != 0) {
				assert(result.size != 0);
			} else {
				assert(result.size == 0);
			}

			SizedBuffer expected_result = buffer_from_string(test_case.raw);

			assert(result == expected_result);

			free_sized_buffer(result);
		}
	}
}

static void BM_base64_enc(benchmark::State& state) {

	std::string base64_provider = get_base64_provider();
	assert(base64_provider == "openssl");

	for(auto _ : state) {
		// This code gets timed

		for(const auto& test_case : base64_test_cases) {

			SizedBuffer input = buffer_from_string(test_case.raw);

			char* result = base64_encode_buffer(input);

			assert(result != nullptr);

			std::string result_str{ result };

			if(input.size != 0) {
				assert(result_str.size() != 0);
			} else {
				assert(result_str.size() == 0);
			}

			const std::string& expected_result = test_case.base64;

			assert(result == expected_result);

			free(result);
		}
	}
}

#ifdef _SIMPLE_SERVER_HAVE_BCRYPT

namespace {
struct TestCaseBaseBcrypt {
	std::string name;
	std::string password;
	HashSaltSettings settings;
};

} // namespace

	#define BCRYPT_DEFAULT_WORK_FACTOR_FOR_TESTS 10

static void BM_bcrypt(benchmark::State& state) {

	std::vector<TestCaseBaseBcrypt> test_cases = {
		{ .name = "normal password (bcrypt)",
		  .password = "password123",
		  .settings = { .work_factor = BCRYPT_DEFAULT_WORK_FACTOR_FOR_TESTS,
		                .use_sha512 = false } },
		{ .name = "normal password (sha512)",
		  .password = "hello world",
		  .settings = { .work_factor = BCRYPT_DEFAULT_WORK_FACTOR_FOR_TESTS, .use_sha512 = true } },
		{ .name = "long password (bcrypt)",
		  .password = "hello world this is a really long password",
		  .settings = { .work_factor = BCRYPT_DEFAULT_WORK_FACTOR_FOR_TESTS,
		                .use_sha512 = false } },
		{ .name = "long password (sha512)",
		  .password = "hello world this is a really long password",
		  .settings = { .work_factor = BCRYPT_DEFAULT_WORK_FACTOR_FOR_TESTS, .use_sha512 = true } },
		{ .name = "utf8 password (bcrypt)",
		  .password = R"(😎😎😎😎😎😎🌍Hello test long string)",
		  .settings = { .work_factor = BCRYPT_DEFAULT_WORK_FACTOR_FOR_TESTS,
		                .use_sha512 = false } },
		{ .name = "utf8 password (sha512)",
		  .password = R"(😎😎😎😎😎😎🌍Hello test long string)",
		  .settings = { .work_factor = BCRYPT_DEFAULT_WORK_FACTOR_FOR_TESTS, .use_sha512 = true } },
	};

	for(auto _ : state) {
		// This code gets timed

		for(const auto& test_case : test_cases) {

			HashSaltResultType* result =
			    hash_salt_string(test_case.settings, test_case.password.c_str());

			assert(result != nullptr);

			{

				bool matches = is_string_equal_to_hash_salted_string(
				    test_case.settings, test_case.password.c_str(), result);

				assert(matches);
			}

			{
				std::string not_my_password = "not my password";

				bool matches2 = is_string_equal_to_hash_salted_string(
				    test_case.settings, not_my_password.c_str(), result);

				assert(!matches2);
			}

			{

				HashSaltSettings wrong_seettings = { .work_factor = test_case.settings.work_factor,
					                                 .use_sha512 = !test_case.settings.use_sha512 };

				bool matches3 = is_string_equal_to_hash_salted_string(
				    wrong_seettings, test_case.password.c_str(), result);

				assert(!matches3);
			}

			free_hash_salted_result(result);
		}
	}
}

#endif

BENCHMARK(BM_sha1)->Name("hash/sha1");

BENCHMARK(BM_base64_dec)->Name("hash/base64/dec");

BENCHMARK(BM_base64_enc)->Name("hash/base64/enc");

BENCHMARK(BM_bcrypt)->Name("hash/bcrypt");
