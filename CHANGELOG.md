# Changelog
All notable changes to Wrapland will be documented in this file.
## [0.526.0-beta.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.525.0-beta.0...wrapland@0.526.0-beta.0) (2022-10-10)


### ⚠ BREAKING CHANGES

* **server:** KDE idle API changes
* **server:** create Plasma windows without parent, unmap them via dtor call
* **client:** dmabuf modifiers in 64 bit only
* **client:** dmabuf formats in vector
* **server:** dmabuf modifier set once per buffer, not for each plane

### Features

* implement plasma_shell applet popup role ([86fecaa](https://gitlab.com/kwinft/wrapland/commit/86fecaa4d9e05e6416fc113eb66d5758cdce530e))
* implement plasma_shell open under cursor request ([5cf1ede](https://gitlab.com/kwinft/wrapland/commit/5cf1edef027878292c57cb940bb19105ab7d1e14))
* **server:** set one modifier per dmabuf buffer ([c3e8005](https://gitlab.com/kwinft/wrapland/commit/c3e800520e63d149a0cdcb1cc5abf7fd147c1425))
* **server:** unmap Plasma windows with dtor call ([a910169](https://gitlab.com/kwinft/wrapland/commit/a9101698d38c99d3558f726c933aa98c9f20ad60))
* support org_kde_plasma_activation_feedback interface ([83a0d38](https://gitlab.com/kwinft/wrapland/commit/83a0d3800f9cb8b1de280fa1924279ed3705fbfa))
* update to Plasma window management version 15 ([9675128](https://gitlab.com/kwinft/wrapland/commit/9675128d0e25e0a903811355ce182198f9324b6c))
* update to Plasma window management version 16 ([68e9200](https://gitlab.com/kwinft/wrapland/commit/68e92002e226641f3fc8e735e677cd5ef451c229))


### Bug Fixes

* bump org_kde_plasma_window_management version ([8bd1320](https://gitlab.com/kwinft/wrapland/commit/8bd1320311139a3d0857312f5ee776635523e61c))
* bump Plasma shell protocol version ([5007208](https://gitlab.com/kwinft/wrapland/commit/5007208f411551c99f62ab3d7f103ef61fdaa05c))
* **client:** remove timer from windowCreated ([421e811](https://gitlab.com/kwinft/wrapland/commit/421e81197a1b2421531773548a97d9fd5a027c21))
* **client:** silence PlasmaWindowManagement clang warnings ([599da6f](https://gitlab.com/kwinft/wrapland/commit/599da6fea165774ed01aba8f3e2212c392b31210))
* replace emit with Q_EMIT ([8eb9935](https://gitlab.com/kwinft/wrapland/commit/8eb9935c066c662c805ffeba1f092016d8b94b9f))
* **server:** check C-function return values ([6ca253a](https://gitlab.com/kwinft/wrapland/commit/6ca253ae00dbae2266c2d581dccd64d02dcb2cc8))
* **server:** correct typo in Wayland::Global ([bcbf01f](https://gitlab.com/kwinft/wrapland/commit/bcbf01f0c504a2fe196fe03ff8761653067bb178))
* **server:** make PlasmaVirtualDesktop dtor public ([306bc77](https://gitlab.com/kwinft/wrapland/commit/306bc77ae99c00181f84d8089869bf498004d4f4))
* **server:** remove duplicate include ([c394f20](https://gitlab.com/kwinft/wrapland/commit/c394f20b91ec7d35b7a7c76ec6404872b0e42db1))
* **server:** remove unused attribute ([9fcd0cb](https://gitlab.com/kwinft/wrapland/commit/9fcd0cb724fbaec679c19cb9c8e5767ee300a9a8))
* **server:** use default member initializer ([ef7ff71](https://gitlab.com/kwinft/wrapland/commit/ef7ff71c949c48ca91f8f295adcf6ebf88d68b61))


### Refactors

* **client:** have dmabuf modifiers as 64 bit in public API ([6d85ea9](https://gitlab.com/kwinft/wrapland/commit/6d85ea9dc8f7fd4f5768f11c415e7f09d4b49d12))
* **client:** store DRM formats in vector ([19ab68e](https://gitlab.com/kwinft/wrapland/commit/19ab68e4ad4779bf3c043187d627f4384fb772cf))
* **server:** publish KDE idle timeout objects ([8b4ee06](https://gitlab.com/kwinft/wrapland/commit/8b4ee06dc0d94cd5a1eb4421a3f77dc152fd2847))
* **server:** replace C-style casts with static casts ([1664423](https://gitlab.com/kwinft/wrapland/commit/166442372742796de34ad4ab4f7fa144844db837))

## [0.525.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.525.0-beta.0...wrapland@0.525.0) (2022-06-14)

## [0.525.0-beta.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.524.0-beta.0...wrapland@0.525.0-beta.0) (2022-06-12)


### ⚠ BREAKING CHANGES

* **client:** output pointer removed from mode struct
* **client:** output device modes are provided as std::vector
* **server:** dmabuf API changes

### Features

* **client:** remove output pointer in mode type ([88280c2](https://gitlab.com/kwinft/wrapland/commit/88280c2a06be386ef6e02fe5e85f003727301393))
* **server:** close dmabuf buffer plane fds ([5159ee4](https://gitlab.com/kwinft/wrapland/commit/5159ee4efdde38ca506cd4778d5bdf8dd96d9722))
* **server:** make dmabuf buffer class a POD-like type ([9dc0454](https://gitlab.com/kwinft/wrapland/commit/9dc04544fcaa780669511b6c12f73b690b715726))
* **server:** provide dmabuf import as std::function ([47801c8](https://gitlab.com/kwinft/wrapland/commit/47801c84d33d7d3c99f64b47e98da510372a47f1))
* **server:** receive dmabuf buffer as smart pointer ([8048153](https://gitlab.com/kwinft/wrapland/commit/80481531d821fc7cecb13a25136ebb3c268208f0))
* **server:** store dmabuf buffer planes and flags ([2b8fdc0](https://gitlab.com/kwinft/wrapland/commit/2b8fdc0cbbcab842c074686955042683372abb66))


### Bug Fixes

* add wayland-client-protocol.h in virtual_keyboard_v1.cpp ([4134b7a](https://gitlab.com/kwinft/wrapland/commit/4134b7a5cf54cfaa8f9524bda74c075e864a1d05))
* **client:** use STL vector for iterator correctness ([f203e3a](https://gitlab.com/kwinft/wrapland/commit/f203e3a41817bb27debf60fbd47f8e71b596bfc9))
* link testLinuxDmabuf with Wayland::Client ([7342423](https://gitlab.com/kwinft/wrapland/commit/73424230b47531d8b17a53e7e0a580408d973aef))
* resolve xml DTD issues ([bb351ba](https://gitlab.com/kwinft/wrapland/commit/bb351ba3028a63ccb242d5818b64aaef99f8fb7c))
* **server:** accept buffer damage unconditionally ([758d8f0](https://gitlab.com/kwinft/wrapland/commit/758d8f01ab069c5f30905392b55fb7961024cc50))
* **server:** check global on dmabuf buffer creation ([4b7da9d](https://gitlab.com/kwinft/wrapland/commit/4b7da9d4b48f5fa8f7f51e4da52216646a164b21))
* **server:** drop "wayland-server.h" in seat header ([94b3fa5](https://gitlab.com/kwinft/wrapland/commit/94b3fa59536edfe7de956f77a2b6394c0289160b))
* **server:** remove QObject parent argument ([5b6bfb7](https://gitlab.com/kwinft/wrapland/commit/5b6bfb7359bc2cb037a55f415baf073816c5ad1e))
* **server:** workaround Xwayland issue with invalid and linear mods ([c03d426](https://gitlab.com/kwinft/wrapland/commit/c03d4261ce67ab9920c2cf4f72a58276ee155756))


### Refactors

* add internal dmabuf buffer resource class ([b385b02](https://gitlab.com/kwinft/wrapland/commit/b385b02b465a2da4c11fdad2d16ef8f3b873bd87))
* **server:** set dmabuf formats with drm_format struct ([5321d5d](https://gitlab.com/kwinft/wrapland/commit/5321d5d41498c0f0a0ec8a67171450418ca7b98c))

## [0.524.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.524.0-beta.0...wrapland@0.524.0) (2022-02-08)

## [0.524.0-beta.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.523.0-beta.0...wrapland@0.524.0-beta.0) (2022-02-03)


### ⚠ BREAKING CHANGES

* **server:** legacy object from resource getters are removed
* **server:** Plasma windows are retrieved in STL vector
* **server:** virtual desktops identified by STL strings
* **server:** virtual desktops getter returns STL vector instead of QList
* **server:** key pressed/released calls replaced with single key call
* **server:** text-input v2 API changes
* **server:** keymaps are set as C strings
* **server:** compositors must send frame events
* **server:** globals are created as smart pointers by the display
* **server:** socket name setter signature changed
* **server:** dnd action enum definition relocated
* **server:** proxy remote surfaces removed
* **server:** drag surface changed signal removed
* **server:** data devices lose selection changed argument

### Features

* **client:** add support for virtual_keyboard_unstable_v1 ([0596cef](https://gitlab.com/kwinft/wrapland/commit/0596cefc871de68ac8a93dfe7b0c2b17e5de9a02))
* **client:** implement wlr_data_control_unstable_v1 ([2d62aab](https://gitlab.com/kwinft/wrapland/commit/2d62aabc45e4419b167aeba54f8236baa118e252))
* **client:** remove data sharing selection cleared signals ([f8c4fa6](https://gitlab.com/kwinft/wrapland/commit/f8c4fa64abf8176cc6ff553df2f44e61ffdb5f4f))
* **server:** add container helper macros ([f524540](https://gitlab.com/kwinft/wrapland/commit/f524540195ae22adf7ba70d8cdb7575178645943))
* **server:** add dropped payload to drag ended signal ([d9575ff](https://gitlab.com/kwinft/wrapland/commit/d9575ff5ce455d696d898c4f1353f71a4afa8dd5))
* **server:** add external data sharing sources ([7369617](https://gitlab.com/kwinft/wrapland/commit/736961724c849c81ead42afe55d09e798882f9b1))
* **server:** add pointer frame function ([dd15cd2](https://gitlab.com/kwinft/wrapland/commit/dd15cd26dfb521450e8a393b393499cf463c4b92))
* **server:** add source actions send function ([8f31559](https://gitlab.com/kwinft/wrapland/commit/8f31559c25926171bc798f664ee403ba7b434931))
* **server:** add support for virtual_keyboard_unstable_v1 ([c39f41a](https://gitlab.com/kwinft/wrapland/commit/c39f41a72dae12e79567031d6e17eeb965f45923))
* **server:** allow to create DRM lease connector without an output ([ec288d5](https://gitlab.com/kwinft/wrapland/commit/ec288d563233c8de85403eee296bd73c1f09ecb8))
* **server:** expand data device API ([c34a2af](https://gitlab.com/kwinft/wrapland/commit/c34a2afdfd5dbf7d10486e789d9b3a04a51298ed))
* **server:** identify virtual desktops with STL strings ([e1797aa](https://gitlab.com/kwinft/wrapland/commit/e1797aa3a5153142ef5b516af13a7022497dbf28))
* **server:** implement wlr_data_control_unstable_v1 ([9a49860](https://gitlab.com/kwinft/wrapland/commit/9a498604cb49677b018270505a5b5a0409ea84af))
* **server:** provide getter for input-method popups ([219e266](https://gitlab.com/kwinft/wrapland/commit/219e26623c444c221a35fd3e49a68ccfaa75ab44))
* **server:** remove data sharing selection cleared signals ([fcfcb7c](https://gitlab.com/kwinft/wrapland/commit/fcfcb7ca4cc49c5f30f34d718006fe6ff475443c))
* **server:** remove drag surface changed signal ([26d6e22](https://gitlab.com/kwinft/wrapland/commit/26d6e22fb6a0e0813faf457f9b517449d951e166))
* **server:** remove EGLStream controller interface ([afd0993](https://gitlab.com/kwinft/wrapland/commit/afd0993608650b4eca7c4c02281839d95e1ecf78))
* **server:** remove legacy resource getter functions ([3088e8f](https://gitlab.com/kwinft/wrapland/commit/3088e8f48f7789194d792033197ae5eaaf565237))
* **server:** remove selection changed payload ([7b6c151](https://gitlab.com/kwinft/wrapland/commit/7b6c1519492729f4df4e5c8147f265f3f5f6ae11))
* **server:** replace proxy remote surfaces with drag movement block ([6e542ee](https://gitlab.com/kwinft/wrapland/commit/6e542eec2f919bd41b7101c62c01cd51724ab7a5))
* **server:** set keymap as raw C string ([ad4f397](https://gitlab.com/kwinft/wrapland/commit/ad4f3970bdc34dabc43779dad933353febae12ff))
* **server:** signal fake input device destruction ([2cff939](https://gitlab.com/kwinft/wrapland/commit/2cff939b50708d8a5731a93966e9ae6793b6da18))
* **server:** store Plasma windows in STL vector ([826458b](https://gitlab.com/kwinft/wrapland/commit/826458b2612c9fb90081ee97a1c3bd889631f935))
* **server:** store virtual desktops in STL vector ([f0f4507](https://gitlab.com/kwinft/wrapland/commit/f0f4507a321f46cbef1e7c7a85f52176c17c8842))
* **server:** sync input-method v2 state to text-input v3 ([e449069](https://gitlab.com/kwinft/wrapland/commit/e449069aa1824a9f6cee3d2142ccd91d87879087))
* **server:** sync text-input v2 to input-method v2 ([3670431](https://gitlab.com/kwinft/wrapland/commit/367043141b418bd225d916b5dab68e738de5299c))
* **server:** sync text-input v3 state to input-method v2 ([b5d99c8](https://gitlab.com/kwinft/wrapland/commit/b5d99c8e9a7f1aed8a64272590e7628e1f5c70c8))


### Bug Fixes

* **client:** name correct input-method grab destructor request ([02c4431](https://gitlab.com/kwinft/wrapland/commit/02c4431f99b070d944480510ea37e11a70e0d05d))
* **server:** always create xdg-output ([45eb0a5](https://gitlab.com/kwinft/wrapland/commit/45eb0a5370d54c232cbfb028e3d96785f8c82760))
* **server:** assert on input capability when accessing device pool ([dd3f40a](https://gitlab.com/kwinft/wrapland/commit/dd3f40a857c2c01a6a4227deb5dca90276e0640a))
* **server:** assume input devices are created and ensure they are removed ([be3e472](https://gitlab.com/kwinft/wrapland/commit/be3e472f6cbcbf70fefe9fd880553b05cfd2fa5c))
* **server:** cancel old selection source after setting new one ([7343c2c](https://gitlab.com/kwinft/wrapland/commit/7343c2c109d3713134de98eef055e2a7a5ab051b))
* **server:** check for protocol errors before converting dnd actions ([7d6c5b4](https://gitlab.com/kwinft/wrapland/commit/7d6c5b4cf2c656e8392899ff516f5fdb80eb4aca))
* **server:** do not reenter surfaces ([74214b8](https://gitlab.com/kwinft/wrapland/commit/74214b8c334a55a0389582ce3c10b72365f7a1f6))
* **server:** ensure device pools are only moved ([e6dac90](https://gitlab.com/kwinft/wrapland/commit/e6dac90c9ad424cd2bf65a169d6b8e460412d7a9))
* **server:** explicitly cast widening conversion ([6515950](https://gitlab.com/kwinft/wrapland/commit/6515950952a7b0ee3ee3d2afe24aba89af5e9424))
* **server:** guard finishCallback against being deleted during transfer ([6a06e78](https://gitlab.com/kwinft/wrapland/commit/6a06e787b2768d5d73f7dfe999337075c012e737))
* **server:** handle panels set to "Windows can cover" ([410ad72](https://gitlab.com/kwinft/wrapland/commit/410ad7241bb2480170d5fc6c93cb8d69bba4f17d))
* **server:** initialize local variable ([a4aa050](https://gitlab.com/kwinft/wrapland/commit/a4aa050a1c1958762182529d1e64ff815e928612))
* **server:** interchange signal argument names ([6c4cc29](https://gitlab.com/kwinft/wrapland/commit/6c4cc293fb246396db6d955a50c065ad4af3e94a))
* **server:** no-lint missing Linux dmabuf request ([a93f2ef](https://gitlab.com/kwinft/wrapland/commit/a93f2ef783b655c95df8e1d413bab7ca2c11b19e))
* **server:** only emit setActionsCallback if DnD actions change ([647db79](https://gitlab.com/kwinft/wrapland/commit/647db79ed0f1c69e095af2f737ab6a11cf471083))
* **server:** prepare server side destroy of DRM lease device binds ([8767af1](https://gitlab.com/kwinft/wrapland/commit/8767af17713241f128bc2c708216406dd91d6063))
* **server:** remove DRM lease device binds on unbind ([69adcd6](https://gitlab.com/kwinft/wrapland/commit/69adcd65179365c982097f4b34a1f2cbc1acdffc))
* **server:** remove unnecessary include ([88da2d8](https://gitlab.com/kwinft/wrapland/commit/88da2d8ef8222fda67d53f1a99708e93fce306e6))
* **server:** remove unnecessary return statement ([78f0240](https://gitlab.com/kwinft/wrapland/commit/78f0240aedbae2ea9e9c90cb4a79a2b15ed05a7a))
* **server:** reset input-method state updates on commit ([894ba38](https://gitlab.com/kwinft/wrapland/commit/894ba382d87b2440be5f3b819aafb580a81e648c))
* **server:** send selections on focus according to protocol ([c5b486a](https://gitlab.com/kwinft/wrapland/commit/c5b486a9112931da3e22e0caf69fe57a52031c6d))
* **server:** set text-input v3 q-ptr ([bba9f60](https://gitlab.com/kwinft/wrapland/commit/bba9f6036427d144fc558bd26fb93407e21214ef))
* **server:** switch between clients with different text-input versions ([311edb3](https://gitlab.com/kwinft/wrapland/commit/311edb36612d404c66c4471c7240744600563639))
* use QString::fromUtf8 on mimetype ([920f8b8](https://gitlab.com/kwinft/wrapland/commit/920f8b8c548bc3030d69a6318f2613f5d1cce95d))
* use remove_all_if instead of invalid erase ([b5b5293](https://gitlab.com/kwinft/wrapland/commit/b5b52933e95940c1ad761c5c250df84b34cdaca0))


### Refactors

* **client:** remove EGLStream client xml ([41b2db1](https://gitlab.com/kwinft/wrapland/commit/41b2db1ccb88de9f9f0ed6e05e4c1dc31989b4d6))
* **server:** add helper function to set data source actions ([5d7cc49](https://gitlab.com/kwinft/wrapland/commit/5d7cc49eed176835b7f52f79dc92ae02a962ab0a))
* **server:** always unbind from global nucleus ([291431f](https://gitlab.com/kwinft/wrapland/commit/291431f1f3202560e8999f466bd08ccb07d26cbe))
* **server:** check data source integrity in non-generic code ([36319ca](https://gitlab.com/kwinft/wrapland/commit/36319ca561d806abf51c7a9f5042cb2ab5ec8db8))
* **server:** cleanup data class includes ([655384a](https://gitlab.com/kwinft/wrapland/commit/655384aea3a9a8ae0b7d2f2598dabe7e7377b7f2))
* **server:** consolidate primary selection source files ([c4a4158](https://gitlab.com/kwinft/wrapland/commit/c4a4158b8f31deb21184301bf483aa574fccdb77))
* **server:** consolidate selection helpers in single header ([b3d5563](https://gitlab.com/kwinft/wrapland/commit/b3d5563c73c199c9f4c864a3688c17a789ebf02c))
* **server:** expose key state enum ([e97d92e](https://gitlab.com/kwinft/wrapland/commit/e97d92ed1561a7eb652d2ef835f6fa6bae056855))
* **server:** get globals with smart pointers ([3b969b2](https://gitlab.com/kwinft/wrapland/commit/3b969b2c8259a92a7bb3cee3742c2520a6ee95b1))
* **server:** handle drags in drag_pool ([c8d6b72](https://gitlab.com/kwinft/wrapland/commit/c8d6b721ae47c1481b8cdfc8507643609b1fb5c7))
* **server:** introduce resource classes for data sources ([da64244](https://gitlab.com/kwinft/wrapland/commit/da64244b0fd2a27185cc9274c8c1bbf9c96694e3))
* **server:** introduce text-input v2 state type ([a04f6b0](https://gitlab.com/kwinft/wrapland/commit/a04f6b00c5d4cb7d02d97305cfd25e6bcd6389fa))
* **server:** make plain data fields plublic in internal classes ([b5e7d13](https://gitlab.com/kwinft/wrapland/commit/b5e7d1337da9e6f786e5035965fbe9c690a20600))
* **server:** merge key pressed and released API ([29e8be1](https://gitlab.com/kwinft/wrapland/commit/29e8be1d735aec4fde50fbbe2aadb5a588ac3402))
* **server:** move dnd action enum to drag pool header ([e7b9ee5](https://gitlab.com/kwinft/wrapland/commit/e7b9ee5635d3264f1aaa7073c0a46ecffbcdfc41))
* **server:** move offer callbacks in classes ([699f014](https://gitlab.com/kwinft/wrapland/commit/699f014dec8f0aae8b5bc5b9567c10e94e60909d))
* **server:** move receive callbacks in classes ([78888c8](https://gitlab.com/kwinft/wrapland/commit/78888c80a1d3f04786fdbfb09dd18ac70ac5ce98))
* **server:** move set selection callbacks in classes ([b2f55b7](https://gitlab.com/kwinft/wrapland/commit/b2f55b781cbf9a92e75472628f0d78a61b5e2d00))
* **server:** remove keyboard_map struct ([495187b](https://gitlab.com/kwinft/wrapland/commit/495187bebaaf1bd5ca634aaf593e43b0c2e59e5a))
* **server:** remove resource check on text-input ([4ac3d1b](https://gitlab.com/kwinft/wrapland/commit/4ac3d1bb2827c01fb67ce6eb03b70ff26a5a730c))
* **server:** remove selection pool advertise function ([d377643](https://gitlab.com/kwinft/wrapland/commit/d37764312ab64f15b89b7e7cd7dc8655dea5afac))
* **server:** remove selection pool update functions ([4c89a72](https://gitlab.com/kwinft/wrapland/commit/4c89a72b008ed73258d163dd8aeeb6e90ff662b8))
* **server:** remove set selection helper functions ([e2277c4](https://gitlab.com/kwinft/wrapland/commit/e2277c450526509088c68ed92da16d3de51ca069))
* **server:** remove xkbcommon-compatible variable ([5fcac58](https://gitlab.com/kwinft/wrapland/commit/5fcac58c6f1c6efa93cf8199bfdd1e216e4830f9))
* **server:** restrict access to selection pool member fields ([6680387](https://gitlab.com/kwinft/wrapland/commit/6680387e8680bfce4706950f2162acb3b6338124))
* **server:** restrict to single socket name setter ([134ca24](https://gitlab.com/kwinft/wrapland/commit/134ca244d3b900ec1000631f1541aacd00189737))
* **server:** return braced initializer lists ([0e36253](https://gitlab.com/kwinft/wrapland/commit/0e362537b8a124e581b912f8b02aca92cdf5c204))
* **server:** send selection always ([34879ad](https://gitlab.com/kwinft/wrapland/commit/34879adeccc6d78b03c1419b5aab7c933c52217c))
* **server:** set selection from update ([6d671b8](https://gitlab.com/kwinft/wrapland/commit/6d671b874dd8e4b41541b10a261581d96c1f1354))
* **server:** set selection sources instead of devices ([cafc9ad](https://gitlab.com/kwinft/wrapland/commit/cafc9adf98dc6249b8f0fd1793ad8651c04e44b4))
* **server:** simplify modifiers changed comparison ([7f281aa](https://gitlab.com/kwinft/wrapland/commit/7f281aa096a442ef33d91d5b381af0732d1a4097))
* **server:** store key states in STL array ([a333ff0](https://gitlab.com/kwinft/wrapland/commit/a333ff097ffe394326d14b2b8b0759d0c21d0580))
* **server:** store Plasma window resources in STL vector ([ca2a062](https://gitlab.com/kwinft/wrapland/commit/ca2a0629afa7f949aef87740d94fc389c00ff11f))
* **server:** store virtual desktop resources in STL vector ([37089fe](https://gitlab.com/kwinft/wrapland/commit/37089fe4d633fa3b89a4a2e2b0dced56e42d09e5))
* **server:** transmit selection in separate function ([ca8a25b](https://gitlab.com/kwinft/wrapland/commit/ca8a25b4aa36d2662eb50069208008e7d4e9aa99))

### [0.523.1](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.523.0...wrapland@0.523.1) (2021-11-30)


### Bug Fixes

* **server:** handle panels set to "Windows can cover" ([c83f317](https://gitlab.com/kwinft/wrapland/commit/c83f317a53380c060f2cf07d77b02da9fb2a9f00))

## [0.523.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.523.0-beta.0...wrapland@0.523.0) (2021-10-14)

## [0.523.0-beta.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.522.0-beta.0...wrapland@0.523.0-beta.0) (2021-10-06)


### ⚠ BREAKING CHANGES

* **server:** surface unmapped signal removed
* **server:** surface property signals are removed
* **server:** individual Surface state getters are removed
* **server:** Surface::damaged signal is removed
* **server:** drag functions are moved from Seat to drag_pool
* **server:** text-input functions are moved from Seat to text_input_pool
* **server:** touch functions are moved from Seat to touch_pool
* **server:** keyboard functions are moved from Seat to keyboard_pool
* **server:** pointer functions are moved from Seat to pointer_pool
* **server:** seat name signal is removed
* **server:** seat capabilities signals are removed
* **server:** signature changes in Server::Seat
* **server:** PlasmaWindow API changes
* **client:** text-input classes are V2 suffixed
* **client:** text-input header file name changes
* **client:** legacy wl_text_input API removed
* DataDeviceManager API changed.
* **server:** Server Slide API changes.

### Features

* **client:** add support for input-method-unstable-v2 ([52899d3](https://gitlab.com/kwinft/wrapland/commit/52899d371a27bba619cf2240dbcbc20405e7084d))
* **client:** add support for text-input-unstable-v3 ([ec9988f](https://gitlab.com/kwinft/wrapland/commit/ec9988fb876fd26e885de447b7203dc8211ff854))
* **client:** add support for the drm_lease_v1 protocol ([2c7d8f0](https://gitlab.com/kwinft/wrapland/commit/2c7d8f03e8d560cbd9ebd48ba392e28354deaa8a))
* **client:** drop support for legacy wl_text_input protocol ([79dc20f](https://gitlab.com/kwinft/wrapland/commit/79dc20fe9a75ba102e9f5e9de5e9d7b001d6f935))
* **client:** provide xdg-activation v1 interface ([045aefd](https://gitlab.com/kwinft/wrapland/commit/045aefdc5d63c241a2faac3e1249dac645a0c634))
* implement wp_primary_selection_unstable_v1 ([2bd7d3f](https://gitlab.com/kwinft/wrapland/commit/2bd7d3fc21070d7279999a17c4d77fdd533a8bc4))
* **server:** add Pointer motion function ([04275a6](https://gitlab.com/kwinft/wrapland/commit/04275a601404c1f870348d58ac1de2443935a810))
* **server:** add support for input-method-unstable-v2 ([3168b4a](https://gitlab.com/kwinft/wrapland/commit/3168b4a8deac3d47ba17f1764bb4a8a0301ed1d9))
* **server:** add support for text-input-unstable-v3 ([b7dcc36](https://gitlab.com/kwinft/wrapland/commit/b7dcc36e040abbe3c2c634a257cc9e23c949ee55))
* **server:** add support for the drm_lease_v1 protocol ([9dda0c7](https://gitlab.com/kwinft/wrapland/commit/9dda0c7683aba632083ce50f8cbd895dec5e2d80))
* **server:** add surface changes bit field ([5d81fae](https://gitlab.com/kwinft/wrapland/commit/5d81faed732051f5b377bd3f2d44b3dd9d3d45b8))
* **server:** expose drag pool ([67dd587](https://gitlab.com/kwinft/wrapland/commit/67dd58709d54b46fa61813648e48aa1727ffbdda))
* **server:** expose keyboard pool ([76e3044](https://gitlab.com/kwinft/wrapland/commit/76e3044d4a584fb3fff87fd0204f27c33f658e74))
* **server:** expose pointer pool ([ee57e5a](https://gitlab.com/kwinft/wrapland/commit/ee57e5a77596f9f059ed8338e450a3ad373afad6))
* **server:** expose text-input pool ([4bad5e6](https://gitlab.com/kwinft/wrapland/commit/4bad5e68db35ee2de8c0eb2f887bc2bb2924c75e))
* **server:** expose touch pool ([9f11178](https://gitlab.com/kwinft/wrapland/commit/9f1117844a56422f8289acd2ef7aed08a1fa7c6a))
* **server:** indicate waiting surface callbacks ([ec42e7d](https://gitlab.com/kwinft/wrapland/commit/ec42e7d303d2cab2d31731d4f49bb7d4c3e57795))
* **server:** introduce public surface state struct ([b49fb33](https://gitlab.com/kwinft/wrapland/commit/b49fb33d6c0bc3d3de0ce50533a106de095af43d))
* **server:** provide xdg-activation v1 interface ([85793a6](https://gitlab.com/kwinft/wrapland/commit/85793a6a0d9d18c060488c519a8875e27183df7e))
* **server:** remove buffer set size function ([ef3de83](https://gitlab.com/kwinft/wrapland/commit/ef3de83e132b3885800fe7fb5c16293b769ffe19))
* **server:** remove capabilities signals ([97e1be7](https://gitlab.com/kwinft/wrapland/commit/97e1be7d349617e2973d6d81e301c653ec6f413e))
* **server:** remove seat name signal ([0a3e695](https://gitlab.com/kwinft/wrapland/commit/0a3e69557662ef9ec3f408d1d77b5360d574ba24))
* **server:** remove surface damaged signal ([02db3bc](https://gitlab.com/kwinft/wrapland/commit/02db3bc29253c45b867e626894c26fcc4d86763c))
* **server:** remove surface property signals ([65faa26](https://gitlab.com/kwinft/wrapland/commit/65faa26f4a81997f7ef3746601339638d972dbc9))
* **server:** send error on Seat capability mismatch ([c31daec](https://gitlab.com/kwinft/wrapland/commit/c31daecbdc8a6c973918c363fefd9bc73423e144))


### Bug Fixes

* **server:** explicitly cast from unsinged to int ([f0dd0a5](https://gitlab.com/kwinft/wrapland/commit/f0dd0a5a0f6a0dec1a9f846a334df5a76cdc5a8b))
* **server:** hold onto appmenus in order to send them to new resources in PlasmaWindow ([39236ff](https://gitlab.com/kwinft/wrapland/commit/39236ff169372473229520a0177b193ff5a9cadc))
* **server:** implement text input entered surface method ([87e87de](https://gitlab.com/kwinft/wrapland/commit/87e87de6c6d92e54230f2e4eb09974fe259f1bfe))
* **server:** indicate seat capabilities through device pool lifetimes ([eeb2c1e](https://gitlab.com/kwinft/wrapland/commit/eeb2c1e3e6861445fa5fe0bae5efdff4c7b885af))
* **server:** omit lint of missing pointer gestures callbacks ([aff4450](https://gitlab.com/kwinft/wrapland/commit/aff4450964a4d63de34201db22adb570f50533ed))
* **server:** provide unsinged slide offset ([48e80fe](https://gitlab.com/kwinft/wrapland/commit/48e80fe020a936ecad22a262c605b765d7a51517))
* **server:** provide updated modifiers serial at right argument position ([b41b274](https://gitlab.com/kwinft/wrapland/commit/b41b274de3e41a68cbd99ebf4fb67f7ad891c226))
* **server:** remove implicit casts ([93fd844](https://gitlab.com/kwinft/wrapland/commit/93fd84498215f3cb1399e6010c5185713a52ad3d))
* **server:** synchronize child surface changes ([84d933d](https://gitlab.com/kwinft/wrapland/commit/84d933d8774529afd1a1b946482d651b1b4a4549))
* **server:** touch move when setting drag target with first key ([23f5b3e](https://gitlab.com/kwinft/wrapland/commit/23f5b3e8a5facb9e6503d2b5d1986ff5bbd3becc))
* set keymap file per wl_keyboard resource ([7abe2a8](https://gitlab.com/kwinft/wrapland/commit/7abe2a8888e6e677bda3628c3bb5c2f9103955b8))


### Refactors

* **client:** abolish private namespace in data private classes ([489e83d](https://gitlab.com/kwinft/wrapland/commit/489e83d3d69c5ca81fd7c63674bbc8f72d49d2e3))
* **client:** bind text-input manager v2 without interface check ([9a0cf23](https://gitlab.com/kwinft/wrapland/commit/9a0cf232ff270fbf56a437366da702ab88801a78))
* **client:** clean up text input code ([c3533da](https://gitlab.com/kwinft/wrapland/commit/c3533da6e51e3a9b49d7ede2a9a4de8fb833c9b8))
* **client:** make DataDevice callbacks free functions ([83f72b6](https://gitlab.com/kwinft/wrapland/commit/83f72b6ea39d1a627cb4f60c2a93adcd397c376d))
* **client:** make DataOffer ctor public ([fc92370](https://gitlab.com/kwinft/wrapland/commit/fc92370b0727114c32a45bb13a43e7938ea79a09))
* **client:** merge text input classes ([73b04ea](https://gitlab.com/kwinft/wrapland/commit/73b04ea3bc978ba76e487c0d7d4a09bbc07e7c7f))
* **client:** merge text input files ([6c0ac94](https://gitlab.com/kwinft/wrapland/commit/6c0ac94b24ac66af161ef79d077e3cd80854ccd7))
* **client:** merge text input manager classes ([df998cd](https://gitlab.com/kwinft/wrapland/commit/df998cd19bd03ede270386cc2fe993496c19354c))
* **client:** move DataOffer callbacks to free functions ([05f6ff8](https://gitlab.com/kwinft/wrapland/commit/05f6ff81a8071809d4c5f9a50e496727c4db8f6c))
* **client:** move some DataSource callbacks to free functions ([794fea7](https://gitlab.com/kwinft/wrapland/commit/794fea72bc2a46d132d62c98248bc4bd71050856))
* **client:** rename text-input files ([ed7ad42](https://gitlab.com/kwinft/wrapland/commit/ed7ad42f91fdf76d6a556371ee83ebf43aee21c8))
* **client:** rename text-input v2 classes ([2bb62ed](https://gitlab.com/kwinft/wrapland/commit/2bb62edd3f80d1bfa414187baa25bcec1afb8aa8))
* rename some DataDeviceManager members ([5c277d9](https://gitlab.com/kwinft/wrapland/commit/5c277d9ed2dbbc24c9a21e7cb495995e788b6d6c))
* **server:** add function to cancel drags ([20ecc14](https://gitlab.com/kwinft/wrapland/commit/20ecc14cd0750cdec34598ce76cb61b11b10f1be))
* **server:** add internal text-input struct depending on version ([82bc29c](https://gitlab.com/kwinft/wrapland/commit/82bc29c458cf1c2e41984e1ce977842fd694bcf8))
* **server:** add Seat friend function ([255db62](https://gitlab.com/kwinft/wrapland/commit/255db6282cb9664ba30c66ed0459008b9d9c18a6))
* **server:** add Seat::Private::register_device template method ([da02ccc](https://gitlab.com/kwinft/wrapland/commit/da02ccc859af904077109d4bda3a4854f4c9afdb))
* **server:** compare keyboard modifiers with operator ([fae4c6d](https://gitlab.com/kwinft/wrapland/commit/fae4c6d76f8e21550ba114a18baa41f687bf98a2))
* **server:** copy current buffer state in separate function ([8bffe27](https://gitlab.com/kwinft/wrapland/commit/8bffe27812fb321176ff9c0c0e6411437c4c9ea7))
* **server:** explicitly cast resource version to integer ([3661b18](https://gitlab.com/kwinft/wrapland/commit/3661b18cc5d7c23b8f3e2de70adca2e5df7792d9))
* **server:** explicitly cast size to unsigned ([0b5a22d](https://gitlab.com/kwinft/wrapland/commit/0b5a22d7f30f186b52a8ce3b33255c357e2c2766))
* **server:** get drag source and offer late ([ac74098](https://gitlab.com/kwinft/wrapland/commit/ac74098c98d2127d82bae139d01032346b1cd2ee))
* **server:** handle seat drags in a new class ([b09a001](https://gitlab.com/kwinft/wrapland/commit/b09a0017966f67fe31e21fac243024c26c2dbc2f))
* **server:** handle seat keyboards in a new class ([7715cbb](https://gitlab.com/kwinft/wrapland/commit/7715cbba1398e27250608f744f80e96e6407343f))
* **server:** handle seat pointers in a new class ([9ddf6ab](https://gitlab.com/kwinft/wrapland/commit/9ddf6abcb807ef3a41827690b5d8823b2a21add4))
* **server:** handle seat touches in a new class ([a1431d3](https://gitlab.com/kwinft/wrapland/commit/a1431d3eba038d946232198ff7e36464afd0e1cd))
* **server:** handle text input in a new class ([43f6325](https://gitlab.com/kwinft/wrapland/commit/43f632587480c634c3a8ca9fd0883eacdae18b9d))
* **server:** improve code quality slightly ([fbcc340](https://gitlab.com/kwinft/wrapland/commit/fbcc34098bbf3b9e0d69351bdf553e9d0c1f69ed))
* **server:** introduce device_manager class template ([7c823c7](https://gitlab.com/kwinft/wrapland/commit/7c823c78ecc9c93b9090b8163112e92a3e0349fc))
* **server:** introduce drag source struct ([26c0260](https://gitlab.com/kwinft/wrapland/commit/26c0260cb672c23cc04f137b4f997894f2094fec))
* **server:** introduce drag target struct ([3852b0b](https://gitlab.com/kwinft/wrapland/commit/3852b0bb49b346d8bd7d49ff8201ba0fc9025658))
* **server:** make DataOffer receive callback a free function ([f9ae510](https://gitlab.com/kwinft/wrapland/commit/f9ae5109dca1073c123cd463013258ec800cbfd4))
* **server:** make input device pools optional ([9a7fdd4](https://gitlab.com/kwinft/wrapland/commit/9a7fdd4d48115765fe048b5c725fe2b6e8c95923))
* **server:** make keyboard focus private with const reference getter ([8266217](https://gitlab.com/kwinft/wrapland/commit/8266217af320104b117e702f83a2db72a909e61d))
* **server:** make pointer focus private with const reference getter ([93a0f78](https://gitlab.com/kwinft/wrapland/commit/93a0f78cd92de984c15102f1bd2d65212a9aa230))
* **server:** make text input struct unnamed ([34fc1f9](https://gitlab.com/kwinft/wrapland/commit/34fc1f9abd22de46dfd4cf4a215af450cb147ca7))
* **server:** make touch focus private with const reference getter ([d14ed3f](https://gitlab.com/kwinft/wrapland/commit/d14ed3f6af1d2cff77c0d8cf32d6c807ed7f2a9a))
* **server:** move DataDevice, DataSource ctors to public ([9d1e464](https://gitlab.com/kwinft/wrapland/commit/9d1e46487c121d589194fdf514682e29128b42cc))
* **server:** move DataDevice::Private selection methods to free functions ([cfa140c](https://gitlab.com/kwinft/wrapland/commit/cfa140cdb18e539f3825f5e0e13c6d40de70b197))
* **server:** move DataDeviceManager callbacks to free function templates ([7402914](https://gitlab.com/kwinft/wrapland/commit/7402914f3fe80a8551670accf7cef633a28cbb4a))
* **server:** move DataSource callback to free function ([0d27dd0](https://gitlab.com/kwinft/wrapland/commit/0d27dd02ce8c8efa60caafa1de221ebb8f75c72b))
* **server:** move some utils to a separate header ([2a3d665](https://gitlab.com/kwinft/wrapland/commit/2a3d66526bc4b4c94f9af7d9f2ad7101ab89eb9c))
* **server:** only set source pointer on drags with pointer ([8b19499](https://gitlab.com/kwinft/wrapland/commit/8b1949981efecd793a03bc9c426391d0738ad501))
* **server:** provide function to move any touch point ([b1a7377](https://gitlab.com/kwinft/wrapland/commit/b1a73779f89d7ea5d040a8e05de50d0e672d9ae8))
* **server:** remove partial Surface state getters ([b90f648](https://gitlab.com/kwinft/wrapland/commit/b90f648f18b14e32339c030489d22c42bed47360))
* **server:** remove sendAxis function ([12e9c94](https://gitlab.com/kwinft/wrapland/commit/12e9c940c6b581f20bdc117962612c61c4314879))
* **server:** remove subsurface check when looping children ([1f5ebcc](https://gitlab.com/kwinft/wrapland/commit/1f5ebcc1b122c658cca4a6b7fbea54feb752c2e4))
* **server:** remove subsurface position workaround ([e265339](https://gitlab.com/kwinft/wrapland/commit/e2653396a7fb7b536fdd47ac5b561a6b46b737be))
* **server:** rename text input manager factory ([fd57ea5](https://gitlab.com/kwinft/wrapland/commit/fd57ea5cf0834e792e5f7d9010dbb6e73b898951))
* **server:** restrict access to drag pool member variables ([01c8476](https://gitlab.com/kwinft/wrapland/commit/01c8476846b3c00bd15d1d86012cfa94c458b7c9))
* **server:** restrict access to keyboard devices variable ([52d140b](https://gitlab.com/kwinft/wrapland/commit/52d140b0406b7c66f60c0de5cc40b595588773ee))
* **server:** restrict access to keyboard pool member variables ([50e5804](https://gitlab.com/kwinft/wrapland/commit/50e5804e9a002b498c8fe1261e3436b4c158b1b5))
* **server:** restrict access to pointer pool devices variable ([5b46494](https://gitlab.com/kwinft/wrapland/commit/5b46494c070481ad65794dbe961d7c0bc7c4fe75))
* **server:** restrict access to pointer pool member variables ([3c9c359](https://gitlab.com/kwinft/wrapland/commit/3c9c35950bdd24ad30d068eec0130f8055eccb5c))
* **server:** restrict access to position variable ([1d96e22](https://gitlab.com/kwinft/wrapland/commit/1d96e223983955c6a28d1bf9a88313eb246fa104))
* **server:** restrict access to touch pool devices variable ([f5bd1c1](https://gitlab.com/kwinft/wrapland/commit/f5bd1c1c5c468d1c8f70b3accf9868bc6ce7c2b8))
* **server:** set capabilities in helper function ([63192ab](https://gitlab.com/kwinft/wrapland/commit/63192ab2ce2768ff05a767058de23f159e9d3b76))
* **server:** set geometry rectangle with static cast ([e3fdc62](https://gitlab.com/kwinft/wrapland/commit/e3fdc625a8845453a5a80404e06ad6927a094ae4))
* **server:** set seat input method from input method manager ([8438638](https://gitlab.com/kwinft/wrapland/commit/84386384ba069bfcc2e41b4669d74e753c704b8c))
* **server:** split out cancelling drag target from update ([90b70de](https://gitlab.com/kwinft/wrapland/commit/90b70de2d40c3a8f14a0034bfeaaf2063275fd8b))
* **server:** split out udpate drag motion ([3ba39c9](https://gitlab.com/kwinft/wrapland/commit/3ba39c90bec6e3883020e4130dd82f3f63e96116))
* **server:** split out updating target offer ([332772e](https://gitlab.com/kwinft/wrapland/commit/332772ea186afd142dfc4f7ddfe3adb1d0ac385e))
* **server:** split selection management to template class ([3873172](https://gitlab.com/kwinft/wrapland/commit/387317217b0c624d15b2b093c659f5455b82e9f0))
* **server:** split up text-input surface setter on version ([27c3157](https://gitlab.com/kwinft/wrapland/commit/27c315739e19030510458a7c916765cc1131ece0))
* **server:** unset source and target drag structs ([f8e2aad](https://gitlab.com/kwinft/wrapland/commit/f8e2aade61cf270dc71c6f92e7bd8df20b569153))
* **server:** update drag motion before setting surface ([29db209](https://gitlab.com/kwinft/wrapland/commit/29db209a0f7d06028c6a46a1f64f21f17d5fbb1f))
* **server:** use range-based for loop ([ae45c83](https://gitlab.com/kwinft/wrapland/commit/ae45c8383e8b8eb0f6d240307311f43f89cd017b))
* **server:** use seat pool getters internally ([4baf4cf](https://gitlab.com/kwinft/wrapland/commit/4baf4cfdd931d8e58c2c420a7ce445d225901212))
* **server:** use std types in Seat ([5841883](https://gitlab.com/kwinft/wrapland/commit/5841883d50f2c0a890f9b3fc2125621df46c7174))
* **server:** use STL min/max on uint32_t types ([f16182e](https://gitlab.com/kwinft/wrapland/commit/f16182ec69332e966c7adb18b8591161227ec178))
* **server:** use surface argument in connect statement ([0db0dcf](https://gitlab.com/kwinft/wrapland/commit/0db0dcf45e34cc9aabb9b896c46132f6ebf8f688))
* **server:** validate dmabuf params in separate function ([6c61ae8](https://gitlab.com/kwinft/wrapland/commit/6c61ae8881af6fd56864007b8bd9a15db88e0ea2))

## [0.522.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.522.0-beta.0...wrapland@0.522.0) (2021-06-08)


### Bug Fixes

* **server:** provide updated modifiers serial at right argument position ([ebcf597](https://gitlab.com/kwinft/wrapland/commit/ebcf597d8aed3bb66f193596ed58fa3ac2f6797b))
* set keymap file per wl_keyboard resource ([9b831db](https://gitlab.com/kwinft/wrapland/commit/9b831dbbd6c411740f218ee4b2a6ce2ec261abf1))


### Refactors

* **server:** compare keyboard modifiers with operator ([77b8056](https://gitlab.com/kwinft/wrapland/commit/77b8056e0e0cddce0921deb6ebdc4016f58104c6))
* **server:** use range-based for loop ([5e6cdc0](https://gitlab.com/kwinft/wrapland/commit/5e6cdc0f62d5ee20b39ae17c72e35eaee8ab169e))

## [0.522.0-beta.0](https://gitlab.com/kwinft/wrapland/compare/wrapland@0.521.0-beta.0...wrapland@0.522.0-beta.0) (2021-05-26)


### ⚠ BREAKING CHANGES

* **client:** The client xdg-shell API interface changes.
* **client:** Client library xdg-shell v6 support removed.

### Features

* **client:** add application menu to org_kde_plasma_window client API ([6424454](https://gitlab.com/kwinft/wrapland/commit/6424454c1037cd013eef7da9fb60d492526c5e7a))
* **client:** add support for wlr_layer_shell_unstable_v1 ([1fa2d50](https://gitlab.com/kwinft/wrapland/commit/1fa2d502359eb7e32a5546e1802fa5a011ec7cdb))
* **client:** provide override to create xdg-popups without a parent ([d7852c0](https://gitlab.com/kwinft/wrapland/commit/d7852c0ed8882dc66b6f9ec9b32428560e01e9c0))
* **client:** remove xdg-shell v6 ([31198d6](https://gitlab.com/kwinft/wrapland/commit/31198d6a0695dd1ffe86809b0107e842f2f05b39))
* **server:** add application menu to org_kde_plasma_window server API ([a3d1794](https://gitlab.com/kwinft/wrapland/commit/a3d1794bae51683c6bc277e0f396e47cd82d1b0c))
* **server:** add support for wlr_layer_shell_unstable_v1 ([f8199a9](https://gitlab.com/kwinft/wrapland/commit/f8199a9b9d7e787f11194fa5e9fb89252e70f57d))
* update org_kde_plasma_window protocol to version 10 ([c546cce](https://gitlab.com/kwinft/wrapland/commit/c546cce300394b5c3d8b7dc275ec8f3bea501613))


### Bug Fixes

* **server:** allow getting xdg-popup without parent ([89cab1f](https://gitlab.com/kwinft/wrapland/commit/89cab1fa5be3cfd0d675516ace057b3e13568cef))
* **server:** check all xdg-shell role creation errors ([f5d0403](https://gitlab.com/kwinft/wrapland/commit/f5d0403a08e283e74160dd0280e68c0d1f689e63))
* **server:** omit sending keyboard leave on client destroy ([c30250a](https://gitlab.com/kwinft/wrapland/commit/c30250a1617c88a06595c74c70dba3ffd849f235))


### Refactors

* **client:** split up xdg-shell files ([9d7d0bc](https://gitlab.com/kwinft/wrapland/commit/9d7d0bc801cc7f4dc95a67c058a3ce53eb342ef5))
* **client:** unvirtualize xdg-shell class ([62702d5](https://gitlab.com/kwinft/wrapland/commit/62702d5acd8edc74d87d15336361dee01fbd4614))
* **client:** unvirtualize xdg-shell popup class ([924baf9](https://gitlab.com/kwinft/wrapland/commit/924baf9c977a4388c000419d386aabe3dd255f12))
* **client:** unvirtualize xdg-shell toplevel class ([948de4e](https://gitlab.com/kwinft/wrapland/commit/948de4e2e91921577ac6427dc5cc70570ed8d0e8))

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
