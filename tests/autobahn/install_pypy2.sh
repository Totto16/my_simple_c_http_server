#!/usr/bin/env bash

# from https://github.com/docker-library/pypy/blob/master/2.7/bookworm/Dockerfile
# modified for ci runs, not docker images

set -eux
set -o pipefail

export PATH="/opt/pypy/bin:$PATH"

# Python 2.7.18
PYPY_VERSION="7.3.19"

DPKG_ARCH="$(dpkg --print-architecture)"
case "${DPKG_ARCH##*-}" in
'amd64')
    url='https://downloads.python.org/pypy/pypy2.7-v7.3.19-linux64.tar.bz2'
    sha256='d38445508c2eaf14ebb380d9c1ded321c5ebeae31c7e66800173d83cb8ddf423'
    ;;
'arm64')
    url='https://downloads.python.org/pypy/pypy2.7-v7.3.19-aarch64.tar.bz2'
    sha256='fe89d4fd4af13f76dfe7315975003518cf176520e3ccec1544a88d174f50910e'
    ;;
'i386')
    url='https://downloads.python.org/pypy/pypy2.7-v7.3.19-linux32.tar.bz2'
    sha256='cc52df02b6926bd8645c1651cd7f6637ce51c2f352d0fb3c6b9330d15194b409'
    ;;
*)
    echo >&2 "error: current architecture ($DPKG_ARCH) does not have a corresponding PyPy $PYPY_VERSION binary release"
    exit 1
    ;;
esac

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


"$SUDO" apt-get update

# install needed things, install before saveing savedAptMark 
"$SUDO" apt-get install -y --no-install-recommends \
    wget \
    ca-certificates \
    bzip2

savedAptMark="$(apt-mark showmanual)"

"$SUDO" apt-get update

# sometimes "pypy" itself is linked against libexpat1 / libncurses5, sometimes they're ".so" files in "/opt/pypy/lib_pypy"
"$SUDO" apt-get install -y --no-install-recommends \
    libexpat1 \
    libncurses6 \
    libncursesw6 \
    libsqlite3-0
# (so we'll add them temporarily, then use "ldd" later to determine which to keep based on usage per architecture)

wget -O pypy.tar.bz2 "$url" --progress=dot:giga
echo "$sha256 *pypy.tar.bz2" | sha256sum --check --strict -
mkdir -p /opt/pypy
tar -xjC /opt/pypy --strip-components=1 -f pypy.tar.bz2
find /opt/pypy/lib* -depth -type d -a \( -name test -o -name tests \) -exec rm -rf '{}' +
rm pypy.tar.bz2

ln -sv '/opt/pypy/bin/pypy' /usr/local/bin/

# smoke test
pypy --version

"$SUDO" apt-mark auto '.*' >/dev/null
# shellcheck disable=SC2086
[ -z "$savedAptMark" ] || "$SUDO" apt-mark manual $savedAptMark >/dev/null

set +o pipefail

TO_MARK_MANUAL="$(find /opt/pypy -type f -executable -exec ldd '{}' ';' | awk '/=>/ { so = $(NF-1); if (index(so, "/usr/local/") == 1) { next }; gsub("^/(usr/)?", "", so); printf "*%s\n", so }' | sort -u | xargs -r dpkg-query --search | cut -d: -f1 | sort -u)"

set -o pipefail

# shellcheck disable=SC2086
"$SUDO" apt-mark manual $TO_MARK_MANUAL >/dev/null

"$SUDO" apt-get purge -y --auto-remove -o APT::AutoRemove::RecommendsImportant=false

# smoke test again, to be sure
pypy --version

find /opt/pypy -depth \
    \( \
    \( -type d -a \( -name test -o -name tests \) \) \
    -o \
    \( -type f -a \( -name '*.pyc' -o -name '*.pyo' \) \) \
    \) -exec rm -rf '{}' +

# https://github.com/pypa/get-pip
PYTHON_GET_PIP_URL="https://github.com/pypa/get-pip/raw/3843bff3a0a61da5b63ea0b7d34794c5c51a2f11/get-pip.py"
PYTHON_GET_PIP_SHA256="95c5ee602b2f3cc50ae053d716c3c89bea62c58568f64d7d25924d399b2d5218"

wget -O get-pip.py "$PYTHON_GET_PIP_URL"
echo "$PYTHON_GET_PIP_SHA256 *get-pip.py" | sha256sum --check --strict -

pipVersion="$(pypy -c 'import ensurepip; print(ensurepip._PIP_VERSION)')"
setuptoolsVersion="$(pypy -c 'import ensurepip; print(ensurepip._SETUPTOOLS_VERSION)')"

pypy get-pip.py \
    --disable-pip-version-check \
    --no-cache-dir \
    "pip == $pipVersion" \
    "setuptools == $setuptoolsVersion" \
    ;

# smoke test
pip --version
find /opt/pypy -depth \
    \( \
    \( -type d -a \( -name test -o -name tests \) \) \
    -o \
    \( -type f -a \( -name '*.pyc' -o -name '*.pyo' \) \) \
    \) -exec rm -rf '{}' +
rm -f get-pip.py
