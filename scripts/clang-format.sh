#!/bin/bash
set -e
CHAOS_ROOT="$(dirname "$0")/.."
COMMON_ARGS="--verbose"
cd "$CHAOS_ROOT"
if [ $# -ge 1 ] ; then
    if [ "$1" == "--dry-run" ] ; then
        find . -type f \( -name "*.h" -or -name "*.hpp" -or -name "*.cpp" \) \
            -and ! -path "*templates*" \
            -and ! -path "*build*" \
            -and ! -path "*.venv*" \
            -exec clang-format $COMMON_ARGS --dry-run -Werror {} \+
    else
        echo "Invalid arguments"
        exit 2
    fi
else
    find . -type f \( -name "*.h" -or -name "*.hpp" -or -name "*.cpp" \) \
        -and ! -path "*templates*" \
        -and ! -path "*build*" \
        -and ! -path "*.venv*" \
        -exec clang-format $COMMON_ARGS -i {} \+
fi
