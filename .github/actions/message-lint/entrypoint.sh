#!/bin/bash

set -eux
set -o pipefail

UPSTREAM_REPO=$1
UPSTREAM_BRANCH=$2
SOURCE_BRANCH=$3

git config --global --add safe.directory $PWD

git status
git remote -v
git branch -v
git rev-list --count HEAD

echo "Source branch: $SOURCE_BRANCH"
echo "Upstream branch: $UPSTREAM_BRANCH"

yarn global add @commitlint/cli
yarn add conventional-changelog-conventionalcommits

# We must unshallow the checkout. On GitHub it's by default only 1 commit deep.
git fetch --unshallow origin $SOURCE_BRANCH

git remote add _upstream $UPSTREAM_REPO || git remote set-url _upstream $UPSTREAM_REPO
git fetch  --depth=1 -q _upstream $UPSTREAM_BRANCH
commitlint --verbose --config=tooling/docs/commitlint.config.js --from=_upstream/$UPSTREAM_BRANCH
