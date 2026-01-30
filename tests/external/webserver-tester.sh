#!/usr/bin/env bash

set -e

git clone https://github.com/ibnesayeed/webserver-tester.git
cd webserver-tester

python3 -m venv venv

source venv/bin/activate

pip3 install -r requirements.txt

python3 ./main.py localhost:8080 cs531a1

# python3 ./main.py localhost:8080
