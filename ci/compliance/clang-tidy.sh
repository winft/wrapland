#!/bin/bash

set -eux
set -o pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
SOURCE_DIR=$(dirname $(dirname ${SCRIPT_DIR}))

python2 $SCRIPT_DIR/run-clang-tidy.py \
    -header-filter="${SOURCE_DIR}/server/" -warnings-as-errors="*" \
    -checks="*, \
        -fuchsia-default-arguments-calls, \
        -fuchsia-default-arguments-declarations, \
        -google-readability-namespace-comments, \
        -llvm-namespace-comment, \
        -llvm-header-guard, \
        -modernize-use-trailing-return-type \
        " \
    ${SOURCE_DIR}/server \
    ${SOURCE_DIR}/autotests/client/output.cpp \
    ${SOURCE_DIR}/autotests/client/seat.cpp \
    ${SOURCE_DIR}/autotests/server/display.cpp
