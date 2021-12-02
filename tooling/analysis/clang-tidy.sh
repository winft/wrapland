#!/bin/bash

set -eux
set -o pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
SOURCE_DIR=$(dirname $(dirname ${SCRIPT_DIR}))
RUN_SCRIPT_URL="https://raw.githubusercontent.com/llvm/llvm-project/release/13.x/\
clang-tools-extra/clang-tidy/tool/run-clang-tidy.py"

if [ $# -eq 0 ] ; then
  BUILD_DIR='build'
else
  BUILD_DIR="$1"
fi

cd $SOURCE_DIR
python <(curl -s $RUN_SCRIPT_URL) \
    -p="$BUILD_DIR" \
    -header-filter="^${SOURCE_DIR}/server/" \
    ${SOURCE_DIR}/server
