

#include <doctest.h>

TEST_CASE("testing parsing of the Accept-Encoding header") {

	SUBCASE("standard simple list") {

		// "Accept-Encoding: compress, gzip",
	}

	SUBCASE("empty value") {
		// "Accept-Encoding:",
	}

	SUBCASE("'*' value") {

		// "Accept-Encoding: *",
	}

	SUBCASE("complicated list with weights") {
		// "Accept-Encoding: compress;q=0.5, gzip;q=1.0",
	}

	SUBCASE("complicated list with weights adn 'identity'") {

		// "Accept-Encoding: gzip;q=1.0, identity; q=0.5, *;q=0",
	}
}
