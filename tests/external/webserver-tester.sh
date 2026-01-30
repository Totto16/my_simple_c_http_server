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
