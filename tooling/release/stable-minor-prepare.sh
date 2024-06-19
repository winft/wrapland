#!/bin/bash

set -eu
set -o pipefail

function set_cmake_version {
    sed -i "s/\(project(Wrapland VERSION \).*)/\1${1})/g" CMakeLists.txt
}

# We always start branching from master.
git checkout master

# Something like: wrapland@0.6XX.0
LAST_TAG=$(git describe --abbrev=0)
LAST_MINOR_VERSION=$(echo $LAST_TAG | sed -e 's,v\(\),\1,g')

echo "Last released minor version was: $LAST_MINOR_VERSION"

# This updates version number from beta to its release
NEXT_MINOR_VERSION=$(semver -i minor $LAST_MINOR_VERSION)

echo "Next minor version: '${NEXT_MINOR_VERSION}'"

# This creates the changelog.
standard-version -t v --skip.commit true --skip.tag true --preMajor --release-as $NEXT_MINOR_VERSION

# Set CMake version.
set_cmake_version $NEXT_MINOR_VERSION

# Now we have all changes ready.
git add CMakeLists.txt CHANGELOG.md

# Commit and tag it.
git commit -m "build: create release ${NEXT_MINOR_VERSION}" -m "Update changelog and raise CMake project version to ${NEXT_MINOR_VERSION}."
git tag -a "v${NEXT_MINOR_VERSION}" -m "Create release ${NEXT_MINOR_VERSION}."

# Now checkout the next stable branch.
BRANCH_NAME="Plasma/$(echo $NEXT_MINOR_VERSION | sed -e 's,^\w.\(\w\)\(0\|\)\(\w*\).*,\1.\3,g')"
echo "New stable branch name: $BRANCH_NAME"
git checkout -b $BRANCH_NAME

# Go back to master branch and update version to next release.
git checkout master
MASTER_CMAKE_VERSION=$(echo $NEXT_MINOR_VERSION | sed -e 's,\w$,80,g')

# Set CMake version
set_cmake_version $MASTER_CMAKE_VERSION

# And commit the updated version.
git add CMakeLists.txt
git commit -m "build: raise CMake project version to ${MASTER_CMAKE_VERSION}" -m "For development on master branch."

echo "Changes applied. Check integrity of master and $BRANCH_NAME branches. Then issue 'git push --follow-tags' on both."
