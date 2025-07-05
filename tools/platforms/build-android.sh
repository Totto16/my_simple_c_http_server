#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status.
set -e
## Treat undefined variables as an error
set -u
# fails if any part of a pipeline (|) fails
set -o pipefail

if [ ! -f "$PWD/README.md" ]; then
    echo "executing script in wrong directory" >&2
    exit 2
fi

export BASE_TOOLCHAIN_PATH="$PWD/tools"

if [ ! -d "$BASE_TOOLCHAIN_PATH/toolchains" ]; then
    mkdir -p "$BASE_TOOLCHAIN_PATH/toolchains"
    echo "*" >"$BASE_TOOLCHAIN_PATH/toolchains/.gitignore"
fi

export NDK_VER_DOWNLOAD="r29-beta2"
export NDK_VER_DESC="r29-beta2"

export BASE_PATH="$BASE_TOOLCHAIN_PATH/toolchains/android-ndk-$NDK_VER_DESC"
export ANDROID_NDK_HOME="$BASE_PATH"
export ANDROID_NDK="$BASE_PATH"

if [ ! -d "$BASE_PATH" ]; then

    LAST_DIR="$(pwd)"

    cd "$BASE_TOOLCHAIN_PATH/toolchains"

    if [ ! -e "android-ndk-$NDK_VER_DOWNLOAD-linux.zip" ]; then

        echo "Downloading android NDK $NDK_VER_DESC"
        wget -q "https://dl.google.com/android/repository/android-ndk-$NDK_VER_DOWNLOAD-linux.zip"
    fi
    unzip -q "android-ndk-$NDK_VER_DOWNLOAD-linux.zip"

    cd "$LAST_DIR"
fi

if [ ! -e "$BASE_PATH/meta/abis.json" ]; then

    echo "no abis.json file found, to determine supported abis" >&2
    exit 2

fi

if [ ! -e "$BASE_PATH/meta/platforms.json" ]; then

    echo "no platforms.json file found, to determine supported platforms and SDKs" >&2
    exit 2

fi

SDK_VERSION=$(jq '.max' -M -r -c "$BASE_PATH/meta/platforms.json")
export SDK_VERSION

mapfile -t ARCH_KEYS < <(jq 'keys' -M -r -c "$BASE_PATH/meta/abis.json" | tr -d '[]"' | sed 's/,/\n/g')

export ARCH_KEYS_INDEX=("${!ARCH_KEYS[@]}")

## options: "smart, complete_rebuild"
export COMPILE_TYPE="smart"

export BUILDTYPE="debug"

if [ "$#" -eq 0 ]; then
    # nothing
    echo "Using all architectures"
elif [ "$#" -eq 1 ] || [ "$#" -eq 2 ] || [ "$#" -eq 3 ]; then
    ARCH=$1

    FOUND=""

    for INDEX in "${!ARCH_KEYS[@]}"; do
        if [[ "$ARCH" == "${ARCH_KEYS[$INDEX]}" ]]; then
            FOUND="$INDEX"
        fi
    done

    if [ -z "$FOUND" ]; then
        echo "Invalid arch: '${ARCH}', supported archs are:" "${ARCH_KEYS[@]}" >&2
        exit 2
    fi

    ARCH_KEYS_INDEX=("$FOUND")

    if [ "$#" -eq 2 ]; then
        COMPILE_TYPE="$2"
    elif [ "$#" -eq 3 ]; then
        COMPILE_TYPE="$2"
        BUILDTYPE="$3"
    fi

else
    echo "Too many arguments given, expected 1, 2 or 3" >&2
    exit 1
fi

if [ "$COMPILE_TYPE" == "smart" ]; then
    : # noop
elif [ "$COMPILE_TYPE" == "complete_rebuild" ]; then
    : # noop
else
    echo "Invalid COMPILE_TYPE, expected: 'smart' or 'complete_rebuild'" >&2
    exit 1
fi

for INDEX in "${ARCH_KEYS_INDEX[@]}"; do
    export KEY=${ARCH_KEYS[$INDEX]}

    RAW_JSON=$(jq '.[$KEY]' -M -r -c --arg KEY "$KEY" "$BASE_PATH/meta/abis.json")

    BITNESS=$(echo "$RAW_JSON" | jq -M -r -c '."bitness"') || true
    ARCH=$(echo "$RAW_JSON" | jq -M -r -c '."arch"')
    ARCH_VERSION=$(echo "$RAW_JSON" | jq -M -r -c '."proc"' | tr -d "-")
    ARM_NAME_TRIPLE=$(echo "$RAW_JSON" | jq -M -r -c '."triple"')
    ARM_TARGET_ARCH=$KEY
    ARM_TRIPLE=$ARM_NAME_TRIPLE$SDK_VERSION
    ARM_COMPILER_TRIPLE=$(echo "$RAW_JSON" | jq -M -r -c '."llvm_triple"')
    ARM_TOOL_TRIPLE=$(echo "$ARM_NAME_TRIPLE$SDK_VERSION" | sed "s/$ARCH/$ARCH_VERSION/")

    export SYM_LINK_PATH=sym-$ARCH_VERSION

    export HOST_ROOT="$BASE_PATH/toolchains/llvm/prebuilt/linux-x86_64"
    export SYS_ROOT="${HOST_ROOT}/$SYM_LINK_PATH/sysroot"
    export BIN_DIR="$HOST_ROOT/bin"
    export PATH="$BIN_DIR:$PATH"

    LIB_PATH="${SYS_ROOT}/usr/lib/$ARM_TRIPLE:${SYS_ROOT}/usr/lib/$ARM_TRIPLE/${SDK_VERSION}"
    INC_PATH="${SYS_ROOT}/usr/include"

    export LIBRARY_PATH="$SYS_ROOT/usr/lib/$ARM_NAME_TRIPLE/$SDK_VERSION"

    if [ "$COMPILE_TYPE" == "complete_rebuild" ] || ! [ -e "$SYS_ROOT" ]; then

        LAST_DIR="$(pwd)"

        if [ -d "${SYS_ROOT:?}/" ]; then

            rm -rf "${SYS_ROOT:?}/"
        fi

        mkdir -p "${SYS_ROOT:?}/usr/lib"

        cd "${SYS_ROOT:?}/usr/"

        ln -s "$HOST_ROOT/sysroot/usr/local" "${SYS_ROOT:?}/usr/"

        ln -s "$HOST_ROOT/sysroot/usr/include" "${SYS_ROOT:?}/usr/"

        find "$HOST_ROOT/sysroot/usr/lib/$ARM_NAME_TRIPLE/" -maxdepth 1 -name "*.so" -exec ln -s "{}" "${SYS_ROOT:?}/usr/lib/" \;

        find "$HOST_ROOT/sysroot/usr/lib/$ARM_NAME_TRIPLE/" -maxdepth 1 -name "*.a" -exec ln -s "{}" "${SYS_ROOT:?}/usr/lib/" \;

        find "$HOST_ROOT/sysroot/usr/lib/$ARM_NAME_TRIPLE/$SDK_VERSION/" -maxdepth 1 -name "*.a" -exec ln -s "{}" "${SYS_ROOT:?}/usr/lib/" \;

        find "$HOST_ROOT/sysroot/usr/lib/$ARM_NAME_TRIPLE/$SDK_VERSION/" -maxdepth 1 -name "*.so" -exec ln -s "{}" "${SYS_ROOT:?}/usr/lib/" \;

        find "$HOST_ROOT/sysroot/usr/lib/$ARM_NAME_TRIPLE/$SDK_VERSION/" -maxdepth 1 -name "*.o" -exec ln -s "{}" "${SYS_ROOT:?}/usr/lib/" \;

        cd "$LAST_DIR"

    fi

    export BUILD_DIR="build-$ARM_TARGET_ARCH"

    export CC="$ARM_TOOL_TRIPLE-clang"
    export CXX="$ARM_TOOL_TRIPLE-clang++"
    export LD="llvm-ld"
    export AS="llvm-as"
    export AR="llvm-ar"
    export RANLIB="llvm-ranlib"
    export STRIP="llvm-strip"
    export OBJCOPY="llvm-objcop"
    export LLVM_CONFIG="llvm-config"
    unset PKG_CONFIG

    ## BUILD dependencies not buildable with meson (to complicated to port)

    ## build openssl with make (meson port is to much work atm, for this feature)

    LAST_DIR="$(pwd)"

    cd "$SYS_ROOT"

    BUILD_DIR_OPENSSL="build-openssl"

    BUILD_OPENSSL_FILE="$SYS_ROOT/$BUILD_DIR_OPENSSL/build_successfull.meta"

    if [ "$COMPILE_TYPE" == "complete_rebuild" ] || ! [ -e "$BUILD_OPENSSL_FILE" ]; then

        mkdir -p "$BUILD_DIR_OPENSSL"

        cd "$BUILD_DIR_OPENSSL"

        OPENSSL_VERSION="3.4.1"

        if [ ! -e "openssl-$OPENSSL_VERSION.tar.gz" ]; then
            wget -q "https://github.com/openssl/openssl/releases/download/openssl-$OPENSSL_VERSION/openssl-$OPENSSL_VERSION.tar.gz"
        fi

        if [ ! -d "openssl-$OPENSSL_VERSION" ]; then
            tar -xzf "openssl-$OPENSSL_VERSION.tar.gz"
        fi

        cd "openssl-$OPENSSL_VERSION"

        OPENSSL_TARGET_ARCH="android-$ARCH"

        export ANDROID_NDK_ROOT="$ANDROID_NDK_HOME"

        if [ "$ARCH_VERSION" = "armv7a" ]; then

            ./Configure --prefix="$SYS_ROOT/usr" no-asm no-tests no-shared "$OPENSSL_TARGET_ARCH" "-D__ANDROID_API__=$SDK_VERSION"
        else
            ./Configure --prefix="$SYS_ROOT/usr" no-tests no-shared "$OPENSSL_TARGET_ARCH" "-D__ANDROID_API__=$SDK_VERSION"
        fi

        make clean

        if [ "$ARCH_VERSION" = "armv7-a" ]; then

            # fix an compile time error since openssl 3.1.0 >
            # see https://github.com/android/ndk/issues/1992
            # see https://github.com/openssl/openssl/pull/22181
            # Apply patch that fixes the armcap instruction

            # sed -e '/[.]hidden.*OPENSSL_armcap_P/d; /[.]extern.*OPENSSL_armcap_P/ {p; s/extern/hidden/ }' -i -- crypto/*arm*pl crypto/*/asm/*arm*pl
            sed -E -i '' -e '/[.]hidden.*OPENSSL_armcap_P/d' -e '/[.]extern.*OPENSSL_armcap_P/ {p; s/extern/hidden/; }' crypto/*arm*pl crypto/*/asm/*arm*pl

        fi

        make -j build_sw

        make install_sw

        touch "$BUILD_OPENSSL_FILE"

    fi

    cd "$LAST_DIR"

    ## END of manual build of dependencies

    MESON_CPU_FAMILY=$ARCH

    ## this is a flaw in the abis.json, everything is labelled with aarch64 and not arm64, but the "arch" json value is wrong, and meson (https://mesonbuild.com/Reference-tables.html#cpu-families) only knows aarch64!
    if [[ $MESON_CPU_FAMILY = "arm64" ]]; then
        MESON_CPU_FAMILY="aarch64"
    fi

    export COMPILE_FLAGS="'--sysroot=${SYS_ROOT:?}','-fPIE','-fPIC','--target=$ARM_COMPILER_TRIPLE','-D__ANDROID_API__=$SDK_VERSION', '-DBITNESS=$BITNESS'"

    export LINK_FLAGS="'-fPIE','-L$SYS_ROOT/usr/lib'"

    cat <<EOF >"$BASE_TOOLCHAIN_PATH/crossbuild-android-$ARM_TARGET_ARCH.ini"
[host_machine]
system = 'android'
cpu_family = '$MESON_CPU_FAMILY'
cpu = '$ARCH_VERSION'
endian = 'little'

[constants]
android_ndk = '$BIN_DIR'
toolchain = '$BIN_DIR/$ARM_TRIPLE'

[binaries]
c = '$CC'
cpp = '$CXX'
c_ld = 'lld'
cpp_ld = 'lld'
ar      = '$AR'
as      = '$AS'
ranlib  = '$RANLIB'
strip   = '$STRIP'
objcopy = '$OBJCOPY'
pkg-config = 'pkg-config'
llvm-config = '$LLVM_CONFIG'

[built-in options]
c_std = 'gnu11'
cpp_std = 'c++23'
c_args = [$COMPILE_FLAGS]
cpp_args = [$COMPILE_FLAGS]
c_link_args = [$LINK_FLAGS]
cpp_link_args = [$LINK_FLAGS]

prefix = '$SYS_ROOT'
libdir = '$LIB_PATH'

[properties]
pkg_config_libdir = '$SYS_ROOT/usr/lib/pkgconfig'
sys_root = '${SYS_ROOT}'

EOF

    export LIBRARY_PATH="$LIBRARY_PATH:$SYS_ROOT/usr/lib/$ARM_NAME_TRIPLE/$SDK_VERSION:$LIB_PATH"

    if [ "$COMPILE_TYPE" == "complete_rebuild" ] || [ ! -e "$BUILD_DIR" ]; then

        meson setup "$BUILD_DIR" \
            "--prefix=$SYS_ROOT" \
            "--wipe" \
            "--includedir=$INC_PATH" \
            "--libdir=$SYS_ROOT/usr/lib/$ARM_NAME_TRIPLE/$SDK_VERSION" \
            --cross-file "$BASE_TOOLCHAIN_PATH/crossbuild-android-$ARM_TARGET_ARCH.ini" \
            "-Dbuildtype=$BUILDTYPE" \
            -Dsecure=enabled \
            -Dcompression_features=zstd,br,deflate,gzip,compress \
            -Dother_features=bcrypt \
            --fatal-meson-warnings

    fi

    meson compile -C "$BUILD_DIR"

    echo -e "Successfully build for android platform ${ARCH}\n"

done
