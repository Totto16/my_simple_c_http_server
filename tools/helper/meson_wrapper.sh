#!/usr/bin/env bash

set -eu

TO_EXECUTE=()

WRAPPER_GENERATE="none"
WRAPPER_GENERATE_CONTENT=""

LINK_FILE="none"
LINK_FILE_INPUT=""
LINK_FILE_OUTPUT=""

for ITEM in "$@"; do
    if [ "$WRAPPER_GENERATE" == "parsing" ]; then

        if [ -n "$WRAPPER_GENERATE_CONTENT" ]; then
            echo "ERROR: wrapper generate content passed twice" >&2
            exit 1
        fi

        WRAPPER_GENERATE_CONTENT="$ITEM"
        WRAPPER_GENERATE="given"
    elif [ "$LINK_FILE" == "parsing" ]; then

        if [ -n "$LINK_FILE_OUTPUT" ]; then
            echo "ERROR: link file argument passed twice" >&2
            exit 1
        fi

        if [ -n "$LINK_FILE_INPUT" ]; then
            LINK_FILE_OUTPUT="$ITEM"
            LINK_FILE="given"
        else
            LINK_FILE_INPUT="$ITEM"
            LINK_FILE="parsing"
        fi

    else

        if [ "$ITEM" == "--wrapper-generate" ]; then

            if [ "$WRAPPER_GENERATE" != "none" ]; then
                echo "ERROR: '--wrapper-generate' passed twice" >&2
                exit 1
            fi

            WRAPPER_GENERATE="parsing"
        elif [ "$ITEM" == "--link-file" ]; then

            if [ "$LINK_FILE" != "none" ]; then
                echo "ERROR: '--link-file' passed twice" >&2
                exit 1
            fi

            LINK_FILE="parsing"
        else
            TO_EXECUTE+=("$ITEM")
        fi
    fi
done

if [ "$WRAPPER_GENERATE" != "given" ]; then
    echo "ERROR: '--wrapper-generate' missing argument" >&2
    exit 1
fi

if [ -z "$WRAPPER_GENERATE_CONTENT" ]; then
    echo "ERROR: no wrapper generate argument passed" >&2
    exit 1
fi

date -Iseconds >"$WRAPPER_GENERATE_CONTENT"

## execute
"${TO_EXECUTE[@]}"

if [ "$LINK_FILE" == "given" ]; then

    if [ -z "$LINK_FILE_INPUT" ]; then
        echo "ERROR: no link file argument passed" >&2
        exit 1
    fi

    if [ -z "$LINK_FILE_OUTPUT" ]; then
        echo "ERROR: no link file argument passed" >&2
        exit 1
    fi

    mkdir -p "$(dirname "$LINK_FILE_OUTPUT")"

    if [ -e "$LINK_FILE_OUTPUT" ]; then
        rm "$LINK_FILE_OUTPUT"
    fi

    ln -sv "$(realpath "$LINK_FILE_INPUT")" "$LINK_FILE_OUTPUT"
fi
