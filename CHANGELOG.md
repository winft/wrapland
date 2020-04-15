# Changelog
All notable changes to Wrapland will be documented in this file.
## [0.518.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.518.0-beta.0...wrapland@0.518.0) (2020-04-15)

## [0.518.0-beta.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.0.0...wrapland@0.518.0-beta.0) (2020-04-01)


### ⚠ BREAKING CHANGES

* **server:** Server API changes.
* **client:** API break of client library.
* **client:** API break of client library.

### Features

* implement wp_viewporter ([568c959](https://gitlab.com/kwinft/wrapland/commit/568c959bbe9c1548333426bb35c1657caef3c9e6))
* replace output device and management protocols ([e9618d0](https://gitlab.com/kwinft/wrapland/commit/e9618d0a78bfeb5fb54e0cc0e5d6b67e68c1cca7))


### Bug Fixes

* **client:** always disconnect wl_diplay ([1843cd5](https://gitlab.com/kwinft/wrapland/commit/1843cd549bd83e2bb46aeda14197cf51c657dd9a))
* **client:** cleanup pending frame callback on destroy ([f5f106a](https://gitlab.com/kwinft/wrapland/commit/f5f106a84e9d6c2ad0384ce86f97068e2f2fd269))
* **client:** disconnect previous wl_display ([db5d87b](https://gitlab.com/kwinft/wrapland/commit/db5d87b75de85e6a05c8db95831c29a7bc896bab))
* **client:** don't destroy the callback on globalsync ([2c1fe3a](https://gitlab.com/kwinft/wrapland/commit/2c1fe3a2b40e144446b47658d3f9ccbbfda590a6))
* **client:** explicitly disconnect event queue signal ([469c9a9](https://gitlab.com/kwinft/wrapland/commit/469c9a9ab5ed0d47de9b3626fe90104e1fe730bc))
* **client:** send changed signal only when finished ([a93039c](https://gitlab.com/kwinft/wrapland/commit/a93039ca8869dbb38cc54da39bcc481058f51dd5))
* **server:** destroy remaining clients before display destroy ([3e71209](https://gitlab.com/kwinft/wrapland/commit/3e712091e7e5fc13fe3a9783fab05c50a4c5952a))
* **server:** emit output destroy signal early ([15dffa0](https://gitlab.com/kwinft/wrapland/commit/15dffa0124e953d32f8bfa62120472523cf10927))
* **server:** manage resource data and unbinds decisively ([f730fe6](https://gitlab.com/kwinft/wrapland/commit/f730fe6275ec286e8bba68e6d599a63420b3451e))
* **server:** remove sub surface early ([46d9824](https://gitlab.com/kwinft/wrapland/commit/46d98244874c7a37d56fc6b507847a697b01026f))
* add output configuration destroy request ([170bef7](https://gitlab.com/kwinft/wrapland/commit/170bef7fe1cbdad292878c58595b5182deab9538))
* close several data leaks ([c6bcec1](https://gitlab.com/kwinft/wrapland/commit/c6bcec11539f253cec0b47eea31ff0c3d3603ac3))
* **server:** ignore SIGPIPE ([6578e3f](https://gitlab.com/kwinft/wrapland/commit/6578e3f6f50bfde8346d9e2adb90329bf6917d02))
* **server:** send all output device data ([52d2b11](https://gitlab.com/kwinft/wrapland/commit/52d2b11a78e770df1f21cd91c59bde0eb3af779a))
* **server:** send output device transform on bind ([7422441](https://gitlab.com/kwinft/wrapland/commit/7422441e0b72593e805c07c519fbac369f72ad19))
* **server:** set buffer offset when attaching to surface ([766c1bd](https://gitlab.com/kwinft/wrapland/commit/766c1bd473bd7e196ec9ab8450fde57b6215f227))
* **server:** unset focused surface on seat destruct ([af41f2a](https://gitlab.com/kwinft/wrapland/commit/af41f2aa857d7a5c0615953814b460f5e47af75c))


### Refactors

* remove deprecated functionality ([2aa36df](https://gitlab.com/kwinft/wrapland/commit/2aa36dfca155d950b5863041dfecdc905241e96d))
* **client:** remove destroy method ([478eca7](https://gitlab.com/kwinft/wrapland/commit/478eca78e5296e9708cf18ee876e7de13824827c))
* **client:** revise destroy logic ([886df8a](https://gitlab.com/kwinft/wrapland/commit/886df8a8e0ceea94acb08b54571cae4a2bb1e934))
* **server:** make private Global create virtual ([0a965fb](https://gitlab.com/kwinft/wrapland/commit/0a965fbe9d9fc6f1d29dfe0015f38abccedbd538))
* **server:** remove xdg-foreign exported signals ([d9c21ad](https://gitlab.com/kwinft/wrapland/commit/d9c21ad3bef733040665ca93863f6a527e50e1b0))
* **server:** remove xdg-foreign imported signals ([411e43b](https://gitlab.com/kwinft/wrapland/commit/411e43b52d8176d20756a670907a443fe8922596))
* **server:** restructure xdg-foreign implementation ([bd75109](https://gitlab.com/kwinft/wrapland/commit/bd75109bf4b258869c1a54e40f009f7878194b59))
* rename project ([821c5cc](https://gitlab.com/kwinft/wrapland/commit/821c5cc061b8df01be6a7699d234bb7a39ff8d67))
