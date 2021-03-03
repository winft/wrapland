# Wrapland CI tooling
## Release
There is a *beta-prepare* and a *stable-prepare* script. At the moment both are to be executed
locally and the new commits then pushed with tags to the server. In both cases the current working
directory must be the source root directory when executing the script.

GitLab release notes must be added afterwards manually. At the moment we just reuse the changelog.
