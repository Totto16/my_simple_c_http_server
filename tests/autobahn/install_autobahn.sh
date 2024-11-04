#!/usr/bin/env bash

set -eux

pypy -m pip install typing

ln -s "$(which "$CC")" /usr/bin/cc

DPKG_ARCH="$(dpkg --print-architecture)"
case "${DPKG_ARCH##*-}" in
'amd64')
    dev_url='http://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl-dev_1.1.1f-1ubuntu2.23_amd64.deb'
    url="http://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2.23_amd64.deb"
    ;;
'arm64')
    echo >&2 "error: arm64 does not have a libssl 1 binary release"
    ;;
'i386')

    dev_url='http://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl-dev_1.1.1f-1ubuntu2.23_i386.deb'
    url="http://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2.23_i386.deb"
    ;;
*)
    echo >&2 "error: current architecture ($DPKG_ARCH) does not have a libssl 1 binary release"
    exit 1
    ;;
esac

wget -O libssl-dev_1.1.1f.deb "$url" --progress=dot:giga
dpkg -i libssl-dev_1.1.1f.deb

wget -O libssl-dev_1.1.1f-dev.deb "$dev_url" --progress=dot:giga
dpkg -i libssl-dev_1.1.1f-dev.deb

pypy -m pip install typing pyopenssl==21.0.0

pypy -m pip install autobahntestsuite==0.8.2
