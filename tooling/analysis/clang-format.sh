#!/bin/bash

set -eux
set -o pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
SOURCE_DIR=$(dirname $(dirname ${SCRIPT_DIR}))
RUN_SCRIPT_URL="https://gitlab.com/kwinft/tooling/-/raw/master/analysis/run-clang-format.py"

python <(curl -s $RUN_SCRIPT_URL) -r ${SOURCE_DIR}
