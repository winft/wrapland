# Changelog
All notable changes to Wrapland will be documented in this file.
## [0.521.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.521.0-beta.0...wrapland@0.521.0) (2021-02-16)

## [0.521.0-beta.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.520.0-beta.0...wrapland@0.521.0-beta.0) (2021-02-07)


### ⚠ BREAKING CHANGES

* **server:** Server library's xdg-shell surface class provides effective
window geometry.
* **server:** Server API expects now consumers to do subsurface input focus
lookup.

### Features

* **client:** add support for xdg-shell v2 ([68e13de](https://gitlab.com/kwinft/wrapland/commit/68e13dec9443ea980a4829be8196158835ee131e))
* **server:** add window geometry check ([09b48ec](https://gitlab.com/kwinft/wrapland/commit/09b48ec7a65b3e43b8216ac5d9beee25351ebf97))
* **server:** add xdg-shell support for v2 ([ac39485](https://gitlab.com/kwinft/wrapland/commit/ac394853eb8c3ce8b811eb8f6542a45b411bff1d))
* **server:** provide call to add socket fd to display ([fb64345](https://gitlab.com/kwinft/wrapland/commit/fb643451af02d091e108219c2b6ac35bb1174ccc))
* **server:** provide effective window geometry from xdg-shell surface ([2aff7a8](https://gitlab.com/kwinft/wrapland/commit/2aff7a8a3aa81375fd17a621ed97cc226442a022))
* **server:** provide window geometry margins ([7364928](https://gitlab.com/kwinft/wrapland/commit/73649283e96ed26296f5893dd3e8ea2d20e74148))


### Bug Fixes

* **client:** clean up wlr output modes with smart pointer ([9786f0f](https://gitlab.com/kwinft/wrapland/commit/9786f0f0d4c92f5a22d56633d58c08ad60a226cf))
* **client:** delete wlr output head on finished callback ([5a4f334](https://gitlab.com/kwinft/wrapland/commit/5a4f334dd030e22d5f9f270f6e6bbce695318d5f))
* **client:** initialize private wlr mode data ([a24ce31](https://gitlab.com/kwinft/wrapland/commit/a24ce318b4a7625ebc72d417075363026dfe5553))
* **client:** make objects non-foreign ([5c97c20](https://gitlab.com/kwinft/wrapland/commit/5c97c207f74459a5361998c2ec8bda14cfc531a5))
* **client:** remove wlr mode when finished ([c42c498](https://gitlab.com/kwinft/wrapland/commit/c42c4982d1aeec4eae1271401c643224e06b5e9b))
* **server:** check for global being removed and destroyed ([37e6238](https://gitlab.com/kwinft/wrapland/commit/37e6238b3a631207b302f98aaf5f81d93cbc2e4e))


### Refactors

* **client:** remove xdg-shell v5 implementation ([d355a85](https://gitlab.com/kwinft/wrapland/commit/d355a85b079998a67092290f3bb4a3c3b8d1061e))
* **server:** define static EGL function pointer with local scope ([8e6e225](https://gitlab.com/kwinft/wrapland/commit/8e6e225feaa546d63ba48735b7ee0de812896898))
* **server:** introduce post_no_memory for globals ([491a4ac](https://gitlab.com/kwinft/wrapland/commit/491a4acee2a1a47ca50edcdd48c7dd4bf0e5bfe1))
* **server:** remove subsurface input focus lookup ([318b3a6](https://gitlab.com/kwinft/wrapland/commit/318b3a69af33e31e7457a55b7f7ab5084e92783d))

## [0.520.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.520.0-beta.1...wrapland@0.520.0) (2020-10-13)

## [0.520.0-beta.1](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.520.0-beta.0...wrapland@0.520.0-beta.1) (2020-10-02)


### Bug Fixes

* **client:** clean up wlr output modes with smart pointer ([ed71ffc](https://gitlab.com/kwinft/wrapland/commit/ed71ffcb0c0bef2a18d00ceb2e40455781c0bd96))
* **client:** delete wlr output head on finished callback ([fa401d7](https://gitlab.com/kwinft/wrapland/commit/fa401d7f2b363650cc5726c386b27f3a2b5ea05a))
* **client:** initialize private wlr mode data ([83ec883](https://gitlab.com/kwinft/wrapland/commit/83ec8831c6cadd382cbf83f599b7f2890c6c439b))
* **client:** make objects non-foreign ([d5631a0](https://gitlab.com/kwinft/wrapland/commit/d5631a0d00b3c7795b00b10a46760cd7b04112c1))
* **client:** remove wlr mode when finished ([784b987](https://gitlab.com/kwinft/wrapland/commit/784b987eb119a34545dccf17fbe7efdcda4baedc))

## [0.520.0-beta.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.519.0-beta.0...wrapland@0.520.0-beta.0) (2020-09-25)


### ⚠ BREAKING CHANGES

* **client:** The client output device mode API changes.
* **server:** The Server Output class is refactored as a single access
point for the compositor.
* Legacy remote-access protocol and API are removed
* **server:** Server Buffer API changes.
* **server:** Server Buffers API changes.

### Features

* support xdg-output v2 ([d58574b](https://gitlab.com/kwinft/wrapland/commit/d58574b10041cbef3f60c7fc2f23070008f73db2))
* support xdg-output v3 ([91d2291](https://gitlab.com/kwinft/wrapland/commit/91d2291e9a69633f17493d8a2f2b221893d5cd2c))
* **client:** simplify current mode logic ([11d29a5](https://gitlab.com/kwinft/wrapland/commit/11d29a5f9958166386bcf2f35319bc7433720468))
* **client:** support zwlr_output_manager_v1 version 2 ([10b49b5](https://gitlab.com/kwinft/wrapland/commit/10b49b59a3cda4c25d14ae9941fdc737b780f949))
* **server:** generate output description ([28367e3](https://gitlab.com/kwinft/wrapland/commit/28367e3da4185e09bc0809f5e11e352dd7abcdca))
* implement keyboard shortcuts inhibit ([132821b](https://gitlab.com/kwinft/wrapland/commit/132821bcabcb6aab766e597d3eb8498e8c0bb969))
* remove remote-access ([054edc8](https://gitlab.com/kwinft/wrapland/commit/054edc8830f4d3289b6e9d140d77f9b36f73c9ee))
* streamline output device information ([fd0194e](https://gitlab.com/kwinft/wrapland/commit/fd0194e994aaf2f6dc56e06c306037a151449f48))
* **server:** add master output class ([bc91afe](https://gitlab.com/kwinft/wrapland/commit/bc91afe899811dff6b78016f74053d14ab926c9a))
* provide method for server-side resource destruction ([ca62f48](https://gitlab.com/kwinft/wrapland/commit/ca62f487c28c37695c31ad0490b97769bbdbee7d))
* support presentation time protocol ([6edb1b9](https://gitlab.com/kwinft/wrapland/commit/6edb1b9e19d64738c8ecef913ce0eb5293123216))
* **client:** implement dmabuf client and autotest ([c4ac0a9](https://gitlab.com/kwinft/wrapland/commit/c4ac0a9020cad6f3c791c87450fb6e1a7dd469e7))


### Bug Fixes

* **client:** convert double to fixed output scale ([c6e9672](https://gitlab.com/kwinft/wrapland/commit/c6e96725c250f6574341a24c062daca47c7ab0a3))
* **client:** disable wlr heads without native check ([26283ca](https://gitlab.com/kwinft/wrapland/commit/26283ca5910b02a5f3f2001b9f653b75197eacba))
* **server:** check for global being removed and destroyed ([3eb7f44](https://gitlab.com/kwinft/wrapland/commit/3eb7f44e09bca11075ac322dcb888762397fb2f9))
* **server:** check for source when accepting offer ([3be21a1](https://gitlab.com/kwinft/wrapland/commit/3be21a13a530f149d16869b4e1d5522f7bb7600b))
* **server:** delay global destroy ([70161be](https://gitlab.com/kwinft/wrapland/commit/70161be4cdd2978dd2bd69aac39c6ac28688518d))
* **server:** ensure modes are stored uniquely ([976a476](https://gitlab.com/kwinft/wrapland/commit/976a4762f0a3a2ee16a7829bcb4615578f092663))
* **server:** free dmabuf private ([714484e](https://gitlab.com/kwinft/wrapland/commit/714484e86e92b59b3d7e1cef6b533995931a98e3))
* **server:** guard wl_output events on since version ([2d574af](https://gitlab.com/kwinft/wrapland/commit/2d574afb2fd957803539d9e2f7700593378cc379))
* **server:** initialize output in dpms off mode ([d7a398b](https://gitlab.com/kwinft/wrapland/commit/d7a398bf74707561ab6ffc2bef54361395a803d0))
* **server:** release global nucleuses after client destroy ([38b8b4a](https://gitlab.com/kwinft/wrapland/commit/38b8b4a46017715a49dc1da3bd69566bd9b2b609))
* **server:** release only surface buffers automatically ([e1fca59](https://gitlab.com/kwinft/wrapland/commit/e1fca5993b81a161036f5cd487e033186cb4ca40))
* **server:** remove drag target on destroy ([3249e3e](https://gitlab.com/kwinft/wrapland/commit/3249e3ebef22b1f0b63f7f2086b2eae838ae14aa))
* **server:** send events on binds only to the bind ([b2bab1a](https://gitlab.com/kwinft/wrapland/commit/b2bab1aa68d4c80c9955e3323e67552c521ac69e))
* **server:** support multiple data devices on a single client ([6770cc5](https://gitlab.com/kwinft/wrapland/commit/6770cc5f848ece837833e2ec46460785fdafccaa))
* **server:** use global bind versions on resource creation ([e498d8f](https://gitlab.com/kwinft/wrapland/commit/e498d8fee618b2ee38862c1f3080a48e7a72f33f))


### Refactors

* **server:** add Buffer private header ([a35d28a](https://gitlab.com/kwinft/wrapland/commit/a35d28a35b44a5cc1216d13989499d079e332329))
* **server:** allow only private access to Buffer make ([ad1effb](https://gitlab.com/kwinft/wrapland/commit/ad1effbe7c9aabec74677a5365a4cf4cc5b93ba5))
* **server:** create internal send function collection ([01d9be6](https://gitlab.com/kwinft/wrapland/commit/01d9be630e267645b58c7c6dcd2fb57eb59e9685))
* **server:** create separate resource bind class ([07ef71a](https://gitlab.com/kwinft/wrapland/commit/07ef71ab4f60e15814e3ce77d4761aa89feaf38a))
* **server:** define xdg-output manager aliases ([7482bcc](https://gitlab.com/kwinft/wrapland/commit/7482bcc6713457ee302b37c7f5c1ce1a31addcf7))
* **server:** introduce nucleus class for globals ([2018f42](https://gitlab.com/kwinft/wrapland/commit/2018f42c50f8133fa272219b0827b87da730a219))
* **server:** introduce ShmImage class ([c7c56e4](https://gitlab.com/kwinft/wrapland/commit/c7c56e404b373f6ede0375322f8142bb571d1d98))
* **server:** introduce SurfaceState struct ([a766515](https://gitlab.com/kwinft/wrapland/commit/a7665154f1456c9f075b8a09d06374ff149ec71a))
* **server:** make SurfaceState a move-only type ([11d0d97](https://gitlab.com/kwinft/wrapland/commit/11d0d97dff8648a3f42ac3e7443bdbe81a9a14b9))
* **server:** modernize subsurface callback ([8e66003](https://gitlab.com/kwinft/wrapland/commit/8e660036c2cc10ac37413134cf5037d1a36877f5))
* **server:** provide Buffers as shared_ptrs ([b62aca1](https://gitlab.com/kwinft/wrapland/commit/b62aca106bdb87e55b514047607ef4991be941a1))
* **server:** remove data offer receive member function ([68a9c4d](https://gitlab.com/kwinft/wrapland/commit/68a9c4da4a0e702d14ce1f2f88e888ae06ce886c))
* **server:** remove global support from Resource ([716340e](https://gitlab.com/kwinft/wrapland/commit/716340e54a395a8df00cf0816b044a412267e490))
* **server:** remove separate shadow creation function ([cd318a7](https://gitlab.com/kwinft/wrapland/commit/cd318a778545ff4f8e5d78c183b7ee593579b3e1))
* **server:** remove unused functions ([1aaee23](https://gitlab.com/kwinft/wrapland/commit/1aaee2366f029be306d8b9fcac3846be3ac8e4cd))
* **server:** rename Output class files ([f84f3ef](https://gitlab.com/kwinft/wrapland/commit/f84f3efa74d18b9d0856a874c475523475804ae4))
* **server:** rename Output class to WlOutput ([2fe2895](https://gitlab.com/kwinft/wrapland/commit/2fe289554b9285c3485cd385a99af990086ca27d))
* **server:** replace capsule with basic nucleus ([f09dc77](https://gitlab.com/kwinft/wrapland/commit/f09dc778b1b14c9f559807b897d2c4b0dea7da32))
* **server:** set entered outputs by wl_output ([3094db3](https://gitlab.com/kwinft/wrapland/commit/3094db39c9379d4d8d912a81d570fc6772a5cbdd))
* **server:** store frame callbacks in deque ([131f70a](https://gitlab.com/kwinft/wrapland/commit/131f70a56795b74dbe32cb9db7b1cea873991535))
* **server:** straighten out Surface and Subsurface state handling ([c38a4e2](https://gitlab.com/kwinft/wrapland/commit/c38a4e234963e1d80cc719e38f00f26dae50ab92))
* **server:** use in-class member initializers ([baedfd3](https://gitlab.com/kwinft/wrapland/commit/baedfd3908128a3a90c1ed03000c99c110ea2daa))
* **server:** use new server model in generator ([64bd019](https://gitlab.com/kwinft/wrapland/commit/64bd019d8b57e2db46196c333ac9b7f4a7e40b21))

## [0.519.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.519.0-beta.0...wrapland@0.519.0) (2020-06-09)


### Bug Fixes

* **server:** support multiple data devices on a single client ([21dcac7](https://gitlab.com/kwinft/wrapland/commit/21dcac7aa9a33034dc9f4a5b923a0052cb5bc35b))

## [0.519.0-beta.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.518.0-beta.0...wrapland@0.519.0-beta.0) (2020-05-24)


### ⚠ BREAKING CHANGES

* The server library has been completely remodelled. See GitLab
issue #14 for more information.

### Features

* **client:** support wlr_output_management_unstable_v1 ([b640d4a](https://gitlab.com/kwinft/wrapland/commit/b640d4a5f9d43271aa498cc1dd592f4be80ba7ca))
* **server:** add a way to get surfaces by id ([06335ad](https://gitlab.com/kwinft/wrapland/commit/06335adef2a9a10056f2ddf0e728aff9cabaeb78))
* **server:** add basic property getters for some classes ([e0f487f](https://gitlab.com/kwinft/wrapland/commit/e0f487f3fa460efc826bb08baac21f1b6c54f759))
* **server:** add Output removed signal ([1bec259](https://gitlab.com/kwinft/wrapland/commit/1bec259650f684dc9e05439d0a16abed3e48095b))


### Bug Fixes

* **client:** add back setting SubSurface member variable ([bc53bec](https://gitlab.com/kwinft/wrapland/commit/bc53bec34b49c374dcf60d0342600097e41a0fe3))
* **client:** build qt 12 0 3 ([0039588](https://gitlab.com/kwinft/wrapland/commit/0039588638c9fe49edab5098ed4ee74e70309d09))
* **server:** act on resourceDestroyed signal ([4853375](https://gitlab.com/kwinft/wrapland/commit/4853375de75693e616ecbaf07d780081ebd4aac0))
* **server:** always set resource implementation ([63766f4](https://gitlab.com/kwinft/wrapland/commit/63766f40d532bf58de9d3c80e7e73a0872f3ffe1))
* **server:** check capsule being valid when removing globals ([488f838](https://gitlab.com/kwinft/wrapland/commit/488f838d6beedabebb492dd1b140356f9dec8afb))
* **server:** check for cursor surface being null ([18e4f24](https://gitlab.com/kwinft/wrapland/commit/18e4f24c3459d7eba284322a21ef01315dbf0f1f))
* **server:** check for drag icon being null ([28838f4](https://gitlab.com/kwinft/wrapland/commit/28838f4ce921756f615de6284dad44da54de623d))
* **server:** check for pointer constraints regions being null ([e7db06b](https://gitlab.com/kwinft/wrapland/commit/e7db06ba52711e8081cb7d56fde396f432aa6f5a))
* **server:** check offer on null mime ([bd26265](https://gitlab.com/kwinft/wrapland/commit/bd2626553f8c1b247393993549eb92ccd1157ff3))
* **server:** check on Plasma window being destroyed ([94ccacb](https://gitlab.com/kwinft/wrapland/commit/94ccacba65564b6b28d1d1aecc2af226071d3368))
* **server:** create dmabuf global, get handle from resource ([3b95945](https://gitlab.com/kwinft/wrapland/commit/3b95945a3eff931a15ffb34aceba2836c3a72630))
* **server:** destroy Private of data device manager ([d744898](https://gitlab.com/kwinft/wrapland/commit/d7448981a3b1e1eb22dc9f968f4e126ef17fd537))
* **server:** destroy Privates in several pointer related classes ([6bd61d7](https://gitlab.com/kwinft/wrapland/commit/6bd61d77ffcc04c549df54c7a8c6b196acb5f3b6))
* **server:** do not move temporary ([2652a06](https://gitlab.com/kwinft/wrapland/commit/2652a0646ee81ca3256729ccd806114c931ce462))
* **server:** enable again compilation with older libwayland ([90a614e](https://gitlab.com/kwinft/wrapland/commit/90a614e6a394d0c9fe231ba35ff0bc609cfce95d))
* **server:** erase-remove buffers correctly ([33d79e6](https://gitlab.com/kwinft/wrapland/commit/33d79e6bd3acf9c61f9a3f98d23301d0e8e735f3))
* **server:** for Surfaces check for null region and leave Outputs correctly ([c8811e0](https://gitlab.com/kwinft/wrapland/commit/c8811e0ccf4639e32a6c84694471a3fd421b602e))
* **server:** get versioned Global from wl_resource ([a773283](https://gitlab.com/kwinft/wrapland/commit/a7732839c0ff99f04400381a9512bd90f34225b9))
* **server:** guard global dtors for display removal ([ffd849c](https://gitlab.com/kwinft/wrapland/commit/ffd849cea91c3ac8a8adef8ac2bab8002e5794a5))
* **server:** handle client disconnect in Pointers ([8435353](https://gitlab.com/kwinft/wrapland/commit/8435353f7788f5fe37859ef7492496add67d178d))
* **server:** handle xdg-foreign resources correctly ([35a65b0](https://gitlab.com/kwinft/wrapland/commit/35a65b0803cb41470add5f333229dc1203b1ccc7))
* **server:** handle xdg-shell teardown robustly ([3212954](https://gitlab.com/kwinft/wrapland/commit/3212954974e9731f423ef955ece9198a6734bcbb))
* **server:** initialize subsurface later ([8bc2d04](https://gitlab.com/kwinft/wrapland/commit/8bc2d04d4ed5fff17fd452dad06fdc24af6a6a0f))
* **server:** move dtor into Capsule member ([b17afbc](https://gitlab.com/kwinft/wrapland/commit/b17afbc0d243a9256e9b97268029a8f5d075c18f))
* **server:** omit flushing the client on drop ([ae2fe9c](https://gitlab.com/kwinft/wrapland/commit/ae2fe9c0ded992fa81de5edd035531140fbf7be7))
* **server:** on Surface resource destroy unfocus pointer ([5890d40](https://gitlab.com/kwinft/wrapland/commit/5890d4028b29a8800d033ee7f0b2ecf952404da3))
* **server:** own Cursor with unique_ptr ([181530c](https://gitlab.com/kwinft/wrapland/commit/181530c5b503dcc4471ec7b6f631706513d5f0fc))
* **server:** own Private with unique_ptr ([bf09888](https://gitlab.com/kwinft/wrapland/commit/bf09888bc13c55d176c7593f4cc46aee6484dede))
* **server:** own Privates with unique_ptr ([fec3390](https://gitlab.com/kwinft/wrapland/commit/fec33907b971a909a74dcfa58fbac142a4297b4e))
* **server:** release global capsules on terminate ([257069b](https://gitlab.com/kwinft/wrapland/commit/257069bb68ecd58c43082698baa08ab9d24d3b2d))
* **server:** remove seat from display list on destroy ([45c10a1](https://gitlab.com/kwinft/wrapland/commit/45c10a1c4cb74778c71cb21626fc6bfb55dd8150))
* **server:** restructure fake input device memory handling ([3a70430](https://gitlab.com/kwinft/wrapland/commit/3a70430bc4f8805cc456161fdbed26e48aae1ab8))
* **server:** return when adding socket fails ([5713b47](https://gitlab.com/kwinft/wrapland/commit/5713b479430e05457b23caa19125b34f839b77c5))
* **server:** set no initial socket name ([e4923b0](https://gitlab.com/kwinft/wrapland/commit/e4923b02107d4276d649fba01f7e2e1e214f50d1))
* **server:** set parent of data device manager ([d63551c](https://gitlab.com/kwinft/wrapland/commit/d63551c7e5a3e503863b4b9b7990c35c16a40769))
* **server:** set parent on relative pointer manager ([16ac291](https://gitlab.com/kwinft/wrapland/commit/16ac291c788433ae60419504b86bd4ef5eaae117))
* **server:** specify the Global bind on init and send ([c3fc45d](https://gitlab.com/kwinft/wrapland/commit/c3fc45d045bc396e37fcca29e8cba8847f02cb68))
* **server:** use variadic function for error posting ([b9e0cd7](https://gitlab.com/kwinft/wrapland/commit/b9e0cd7be9c7b4ce0fa8fad060fefa61ee4b1557))
* encapsulate Wayland globals for release without destruct ([97a9ea7](https://gitlab.com/kwinft/wrapland/commit/97a9ea7af816359c88a47f0d647d614631cd3e12))


### Refactors

* **client:** convert to shared ptr ([ccf7c2d](https://gitlab.com/kwinft/wrapland/commit/ccf7c2d494e9b59d816813101100be60670b47a4))
* **client:** convert to uniqueptr ([ab419fc](https://gitlab.com/kwinft/wrapland/commit/ab419fcbf7542bb533e5c7d09f4a418b46d9ba8e))
* **client:** remove qproperty ([ea0ebcc](https://gitlab.com/kwinft/wrapland/commit/ea0ebcc9a7a5eee3f6e08ff914ffd9b496f7fd2e))
* **server:** add Global binds getter ([9485f59](https://gitlab.com/kwinft/wrapland/commit/9485f59b686f83df8a2a03b87d152cfafac7aa3d))
* **server:** convert QScopedPointer to std::unique_ptr ([45e244d](https://gitlab.com/kwinft/wrapland/commit/45e244df776c84fd054ea7e07fc24a42e71d0284))
* **server:** declare Globals with constexpr version ([3fb4c6c](https://gitlab.com/kwinft/wrapland/commit/3fb4c6c0fef2eb03e0d1220b2a8b64288c8da155))
* **server:** define special member functions ([a70d3cf](https://gitlab.com/kwinft/wrapland/commit/a70d3cf9e7b0fdfe8642f5aa083cd95be3861d92))
* **server:** disable handle of globals resources ([da345d8](https://gitlab.com/kwinft/wrapland/commit/da345d81423db92331f78c187bf68c72f2f2728c))
* **server:** drop deprecated QtSurfaceExtension ([fdcfea1](https://gitlab.com/kwinft/wrapland/commit/fdcfea11036e5bada87f1cbad7f398a870529bad))
* **server:** drop ServerDecoration ([eb686af](https://gitlab.com/kwinft/wrapland/commit/eb686af0e3e5bfc02a9584b4566c3ee9029badb0))
* **server:** get native wl_client through function ([8b10685](https://gitlab.com/kwinft/wrapland/commit/8b10685971711f88c6214de5b6e50723446bcff9))
* **server:** get native wl_display through function ([52fe61a](https://gitlab.com/kwinft/wrapland/commit/52fe61a041942a166ff553a72cf9aec8c36ad29b))
* **server:** improve behavior of deleted special member functions ([a65a0f3](https://gitlab.com/kwinft/wrapland/commit/a65a0f36417fc6cd0461816767e09b1bcf46a72d))
* **server:** improve fake input implementation ([f40f11b](https://gitlab.com/kwinft/wrapland/commit/f40f11b910e0a965fa6a829cd591cfc9c8055b17))
* **server:** improve slightly kde-idle ([e2f646f](https://gitlab.com/kwinft/wrapland/commit/e2f646ffeb259959b928d66eaad6f21259f73a69))
* **server:** initialize all variables ([37abaae](https://gitlab.com/kwinft/wrapland/commit/37abaaea426cf1327918bd349193f99e9558d4a2))
* **server:** let several more clang-tidy checks pass ([02e4b37](https://gitlab.com/kwinft/wrapland/commit/02e4b3739633ab9afc2f0da5596f2d6ace9c43b7))
* **server:** manage subsurface pointers explicitly ([b2e1eae](https://gitlab.com/kwinft/wrapland/commit/b2e1eae28007ec25b089e24878f0782be8da5956))
* **server:** move appmenu to new server model ([0c27e66](https://gitlab.com/kwinft/wrapland/commit/0c27e66bf229d2c7ca8b039a85cbfe2e571c1d04))
* **server:** move Blur to new server model ([ac63499](https://gitlab.com/kwinft/wrapland/commit/ac634997e1aee1dcfb42ff6783d961f029cd82e4))
* **server:** move Contrast to new server model ([ffc560f](https://gitlab.com/kwinft/wrapland/commit/ffc560f60d37c215237a49348682602536789a7a))
* **server:** move decoration palette to new server class ([4ceb3a1](https://gitlab.com/kwinft/wrapland/commit/4ceb3a137dd5f8f844b48b55fa9bdfb546762a30))
* **server:** move eglstream to new server class ([190d0e9](https://gitlab.com/kwinft/wrapland/commit/190d0e99b069715fc14200d9c3ad8e23a0b2edd7))
* **server:** move fakeinput to new server class ([b6bd613](https://gitlab.com/kwinft/wrapland/commit/b6bd613923bab7d1475e895e340a9fb77d555a66))
* **server:** move filtered display to new server model ([f452f1c](https://gitlab.com/kwinft/wrapland/commit/f452f1c941bdc5d4e713e09a652f6c7c2836bb10))
* **server:** move functionality into Private ([d91b0ff](https://gitlab.com/kwinft/wrapland/commit/d91b0ff33aa0de20377808e1368f49c0507c1cca))
* **server:** move idle class to new server model ([41f58e9](https://gitlab.com/kwinft/wrapland/commit/41f58e9922b1880a349f9fd7eec9ae08fb020e8f))
* **server:** move idle-inhibit to new server model ([f8cd731](https://gitlab.com/kwinft/wrapland/commit/f8cd731167fbf2def6cea1d58c91f9390e2e283c))
* **server:** move KeyState to new server model ([b684783](https://gitlab.com/kwinft/wrapland/commit/b684783767e9b149aad7a0308f45ec5416b1125c))
* **server:** move plasmashell to new server class ([1f70428](https://gitlab.com/kwinft/wrapland/commit/1f7042836a1069c8ee5845ef827258f53721ac33))
* **server:** move shadow to new server model ([51130fe](https://gitlab.com/kwinft/wrapland/commit/51130fe4715bc4b5317ca9897af4b583c6b898e6))
* **server:** move Slide to new server model ([3bc37ad](https://gitlab.com/kwinft/wrapland/commit/3bc37ad6d9f74717dea80ee151954b040c4d2dcb))
* **server:** move textinput to new server class ([43a01b4](https://gitlab.com/kwinft/wrapland/commit/43a01b488ec25a75884dbf74ea5ca563c43e45cc))
* **server:** move viewporter to new server model ([bf5e60a](https://gitlab.com/kwinft/wrapland/commit/bf5e60aba4da205c6a50ba7f90c7a44d32e4e05a))
* **server:** move xdgoutput to new server class ([1f80266](https://gitlab.com/kwinft/wrapland/commit/1f8026610520bf7fc3d341cb82dfbcb0236618a8))
* **server:** provide virtual unbind hook ([5f888c9](https://gitlab.com/kwinft/wrapland/commit/5f888c92f544d6287a2a5d92db091bfbf65b9457))
* **server:** register uint32_t in display.h ([561ab18](https://gitlab.com/kwinft/wrapland/commit/561ab18fd0fdb8e62ebf7df606a6b3f730a58069))
* **server:** remodel buffer and dmabuf classes ([960a823](https://gitlab.com/kwinft/wrapland/commit/960a8239a425437cea474d9b793a04c6e1855e54))
* **server:** remodel data sharing classes ([1a8feb2](https://gitlab.com/kwinft/wrapland/commit/1a8feb2f79d7f0b2954bc5b3865a6f27a4a8dc82))
* **server:** remodel output management ([56dae31](https://gitlab.com/kwinft/wrapland/commit/56dae319dee6c6453cdd7c1f2479dbcae91126ea))
* **server:** remodel Plasma window and virtual desktop management ([d20c824](https://gitlab.com/kwinft/wrapland/commit/d20c8242246f394da81fb069fca54915ca2fae65))
* **server:** remodel remote access ([6b34f81](https://gitlab.com/kwinft/wrapland/commit/6b34f81e4332702a414824150013d623497f11c4))
* **server:** remodel surface and related classes ([fb580a3](https://gitlab.com/kwinft/wrapland/commit/fb580a309ce2ac129751550bf798b10b95c156b5))
* **server:** remodel touch class ([1a7342e](https://gitlab.com/kwinft/wrapland/commit/1a7342eab4bc3464c15d9829071a96e9e3dc84f4))
* **server:** remodel xdg-shell ([bd81a23](https://gitlab.com/kwinft/wrapland/commit/bd81a232752e9d6f0eea498e5618587490ff4887))
* **server:** remove legacy infrastructure ([45c9aac](https://gitlab.com/kwinft/wrapland/commit/45c9aac97b399b9e65ceb39fe0e6259d946f2c72))
* **server:** remove legacy Output functions ([64d0bc9](https://gitlab.com/kwinft/wrapland/commit/64d0bc9c017703c60c82f695d3791f9045cfdfca))
* **server:** remove operator access to capsule ([3056a64](https://gitlab.com/kwinft/wrapland/commit/3056a646eac9716b9c5057d7fcdcf39660c8517a))
* **server:** remove OutputInterface class ([0f18ae5](https://gitlab.com/kwinft/wrapland/commit/0f18ae5f319516591b1c1ebc254be60531b60ecd))
* **server:** remove SeatInterface class ([de644ae](https://gitlab.com/kwinft/wrapland/commit/de644aef993c0ab0689ea465a269605624dedbb7))
* **server:** remove static list of clients ([4424786](https://gitlab.com/kwinft/wrapland/commit/4424786da308946fbc462b9933b1ff62dccb1689))
* **server:** remove unneeded constexpr interface if ([8ba74b3](https://gitlab.com/kwinft/wrapland/commit/8ba74b3f015bc07fe5d5f565207e1b9d2ae8dba0))
* **server:** remove unneeded q-ptrs ([8f8e55a](https://gitlab.com/kwinft/wrapland/commit/8f8e55a397970eb419b73323e4f259c032cc39cb))
* **server:** replace destroy callback ([96d122a](https://gitlab.com/kwinft/wrapland/commit/96d122ab01d150ef0d42d2eda5d3ea21344d80f7))
* **server:** replace fromResource with static handle function ([effbcbb](https://gitlab.com/kwinft/wrapland/commit/effbcbba44b14c4fbac94863e82ca15185c2d311))
* **server:** replace or decorate static-capable functions ([2839280](https://gitlab.com/kwinft/wrapland/commit/2839280a3f167931987244610139efed28363324))
* **server:** use smart pointers and respect rule of zero where possible ([5158365](https://gitlab.com/kwinft/wrapland/commit/515836518a45d5277c08f35f57c0a9498be98a1e))
* **server:** use templates for shadow buffers ([32fe450](https://gitlab.com/kwinft/wrapland/commit/32fe450cb07e9a7b9dbb137df6e9c224b9595313))
* cast client through Display ([e6895cf](https://gitlab.com/kwinft/wrapland/commit/e6895cf5b6fc668f2f9828ec82c67fdb8762c186))
* cast Display backend from Private class ([a205802](https://gitlab.com/kwinft/wrapland/commit/a205802eb4499e8e71bc7f4f264e4231277fe1c1))
* cast Display backend from static collection ([fc1dc5d](https://gitlab.com/kwinft/wrapland/commit/fc1dc5d9a36004908ad66e35050a6b3d9fe05c27))
* prototype server objects remodel project ([055bca7](https://gitlab.com/kwinft/wrapland/commit/055bca7af5eb195b4818ec6d87aae98b73293607))
* remodel Dpms classes ([7ea7e72](https://gitlab.com/kwinft/wrapland/commit/7ea7e721ccee77394f037df0b1637c56b06d8b4e))
* remodel input classes ([f04552c](https://gitlab.com/kwinft/wrapland/commit/f04552c20d4375682830bb043fb3a9d11188db12))
* remodel Seat class ([6f59e56](https://gitlab.com/kwinft/wrapland/commit/6f59e56175657de5d61748ffecee5b4f8214be12))
* rename Server::Display type ([3e9b456](https://gitlab.com/kwinft/wrapland/commit/3e9b456c32e3bef272c12e4d230398b93039189d))
* templatify internal send API ([ab760fa](https://gitlab.com/kwinft/wrapland/commit/ab760fabd2462a4c024f2b5f6a40fe48f0a98485))
* templatify Resource and Global classes ([cf872d8](https://gitlab.com/kwinft/wrapland/commit/cf872d8c99269216e442b871874864fd53abcea4))

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
