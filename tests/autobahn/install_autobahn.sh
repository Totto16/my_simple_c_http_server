#!/usr/bin/env bash

set -eux
set -o pipefail

pypy -m pip install typing

# gitlab vs github CI

sudo_wrapper() {
    "$@"
}

# Set SUDO to "sudo" if it's available, else to an empty string
if command -v sudo >/dev/null 2>&1; then
    SUDO="sudo"
else
    SUDO="sudo_wrapper"
fi

if [ -z "${CC:-}" ]; then
    export CC="$GCC"
fi

"$SUDO" ln -s "$(which "$CC")" /usr/bin/cc

DPKG_ARCH="$(dpkg --print-architecture)"
case "${DPKG_ARCH##*-}" in
'amd64')
    dev_url='http://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl-dev_1.1.1f-1ubuntu2.24_amd64.deb'
    url="http://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2.24_amd64.deb"
    ;;
'arm64')
    dev_url='http://ports.ubuntu.com/pool/main/o/openssl/libssl-dev_1.1.1f-1ubuntu2.24_arm64.deb'
    url="http://ports.ubuntu.com/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2.24_arm64.deb"
    ;;
'i386')

    dev_url='http://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl-dev_1.1.1f-1ubuntu2.24_i386.deb'
    url="http://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2.24_i386.deb"
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
