#!/bin/bash

set -eux
set -o pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
SOURCE_DIR=$(dirname $(dirname ${SCRIPT_DIR}))

if [ $# -eq 0 ] ; then
  BUILD_DIR='../build'
else
  BUILD_DIR="$1"
fi

python2 $SCRIPT_DIR/run-clang-tidy.py \
    -p="$BUILD_DIR" \
    -header-filter="${SOURCE_DIR}/server/" \
    ${SOURCE_DIR}/server
