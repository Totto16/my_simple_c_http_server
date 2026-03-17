#!/usr/bin/env bash

set -eu

TO_EXECUTE=()

WRAPPER_GENERATE="false"
WRAPPER_GENERATE_CONTENT=""

for ITEM in "$@"; do
    if [ "$WRAPPER_GENERATE" == "true" ]; then

        if [ -n "$WRAPPER_GENERATE_CONTENT" ]; then
            echo "ERROR: wrapper generate content passed twice" >&2
            exit 1
        fi

        WRAPPER_GENERATE_CONTENT="$ITEM"
        WRAPPER_GENERATE="false"
    else

        if [ "$ITEM" == "--wrapper-generate" ]; then

            if [ "$WRAPPER_GENERATE" == "true" ]; then
                echo "ERROR: '--wrapper-generate' passed twice" >&2
                exit 1
            fi

            WRAPPER_GENERATE="true"
        else
            TO_EXECUTE+=("$ITEM")
        fi
    fi
done

if [ "$WRAPPER_GENERATE" == "true" ]; then
    echo "ERROR: '--wrapper-generate' missing argument" >&2
    exit 1
fi

if [ -z "$WRAPPER_GENERATE_CONTENT" ]; then
    echo "ERROR: not wrapper generate argument passed" >&2
    exit 1
fi

date -Iseconds >"$WRAPPER_GENERATE_CONTENT"

## execute
"${TO_EXECUTE[@]}"
