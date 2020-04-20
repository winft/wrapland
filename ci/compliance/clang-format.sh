#!/bin/bash

set -eux
set -o pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
SOURCE_DIR=$(dirname $(dirname ${SCRIPT_DIR}))

python $SCRIPT_DIR/run-clang-format.py \
    -r ${SOURCE_DIR}/server \
    ${SOURCE_DIR}/autotests/client/data_device.cpp \
    ${SOURCE_DIR}/autotests/client/data_source.cpp \
    ${SOURCE_DIR}/autotests/client/drag_and_drop.cpp \
    ${SOURCE_DIR}/autotests/client/output.cpp \
    ${SOURCE_DIR}/autotests/client/seat.cpp \
    ${SOURCE_DIR}/autotests/client/selection.cpp \
    ${SOURCE_DIR}/autotests/server/display.cpp
