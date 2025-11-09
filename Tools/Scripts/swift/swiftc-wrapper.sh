#!/bin/bash
# cmake accumulates CFLAGS from pkg-config, and then passes them to swiftc.
# One such argument is -pthread which swiftc cannot accommodate. Filter it out.

set -e

REAL_SWIFTC=
args=()

for arg in "$@"; do
    if [ "$arg" != "-pthread" ]; then
        if [[ "$arg" == --original-swift-compiler=* ]]; then
            REAL_SWIFTC="${arg#--original-swift-compiler=}"
        else
            args+=("$arg")
        fi
    fi
done

exec "$REAL_SWIFTC" "${args[@]}"
