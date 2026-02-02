#!/usr/bin/env bash

set -e

git clone https://github.com/ibnesayeed/webserver-tester.git
cd webserver-tester

python3 -m venv venv

source venv/bin/activate

pip3 install -r requirements.txt

mkdir webroot

cd webroot

tar xzf ../sample/cs531-test-files.tar.gz

echo "*" >.gitignore

cd ..

export WEBSERVER_TEST_WEBROOT="$(realpath webroot/)"

python3 ./main.py localhost:8080 cs531a1

# python3 ./main.py localhost:8080

PASS:
python3 ./main.py localhost:8080 cs531a1 test_url_get_ok &
python3 ./main.py localhost:8080 cs531a1 test_url_head_ok
python3 ./main.py localhost:8080 cs531a1 test_path_head_ok
python3 ./main.py localhost:8080 cs531a1 test_path_options_ok
python3 ./main.py localhost:8080 cs531a1 test_get_missing
python3 ./main.py localhost:8080 cs531a1 test_get_duplicate_path_prefix
python3 ./main.py localhost:8080 cs531a1 test_unsupported_version

ERROR:

UNTESTED:

python3 ./main.py localhost:8080 cs531a1 test_invalid_request
python3 ./main.py localhost:8080 cs531a1 test_missing_host_header
python3 ./main.py localhost:8080 cs531a1 test_post_not_implemented
python3 ./main.py localhost:8080 cs531a1 test_trace_echoback
python3 ./main.py localhost:8080 cs531a1 test_get_escaped_file_name
python3 ./main.py localhost:8080 cs531a1 test_get_escape_escaping_character
python3 ./main.py localhost:8080 cs531a1 test_get_jpeg_image
python3 ./main.py localhost:8080 cs531a1 test_get_case_sensitive_file_extension
python3 ./main.py localhost:8080 cs531a1 test_get_empty_text_file
python3 ./main.py localhost:8080 cs531a1 test_get_empty_unknown_file_directory
python3 ./main.py localhost:8080 cs531a1 test_get_filename_with_many_dots
python3 ./main.py localhost:8080 cs531a1 test_get_magic_cookie_of_a_binary_file
python3 ./main.py localhost:8080 cs531a1 test_access_log_as_virtual_uri
