#!/bin/bash

set -eu
set -o pipefail

function get_branch_beta_version {
    echo $(echo $(git describe --abbrev=0) | sed -e 's,wrapland@\(\),\1,g')
}

# We always start the beta branching from master.
git checkout master

# Something like: wrapland@0.5XX.0-beta.0
CURRENT_FIRST_BETA_VERSION=$(get_branch_beta_version)

echo "Current first beta version was: $CURRENT_FIRST_BETA_VERSION"

# Go to stable branch.
BRANCH_NAME="Plasma/$(echo $CURRENT_FIRST_BETA_VERSION | sed -e 's,^\w.\(\w\)\(\w*\).*,\1.\2,g')"
echo "Stable branch name: $BRANCH_NAME"
git checkout $BRANCH_NAME

# Something like: wrapland@0.5XX.0-beta.X
CURRENT_BETA_VERSION=$(get_branch_beta_version)

# This updates the beta version.
NEXT_BETA_VERSION=$(semver -i prerelease $CURRENT_BETA_VERSION)

echo "Next beta version: '${NEXT_BETA_VERSION}'"

# This creates the changelog.
standard-version -t wrapland\@ --skip.commit true --skip.tag true --preMajor --release-as $NEXT_BETA_VERSION

# Now we have all changes ready.
git add CHANGELOG.md

# Commit and tag it.
git commit -m "build: create beta release ${NEXT_BETA_VERSION}" -m "Update changelog."
git tag -a "wrapland@${NEXT_BETA_VERSION}" -m "Create beta release ${NEXT_BETA_VERSION}."

# Go back to master branch and update changelog.
git checkout master
git checkout $BRANCH_NAME CHANGELOG.md
git commit -m "docs: update changelog" -m "Update changelog from branch $BRANCH_NAME at beta release ${NEXT_BETA_VERSION}."

echo "Changes applied. Check integrity of master and $BRANCH_NAME branches. Then issue 'git push --follow-tags' on both."
