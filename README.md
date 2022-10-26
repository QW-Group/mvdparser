# MVDparser: a Quakeworld Multi-View Demos (MVD) Parser


In QuakeWorld and the popular server MVDSV, it is possible to record server side demos that include the views of all players on the map.

This program parses such demos and can output statistics about them in a customized format.

## Supported architectures

The following architectures are fully supported by **[MVDPARSER][mvdparser]** and are available as prebuilt binaries:
* Linux amd64 (Intel and AMD 64-bits processors)
* Linux i686 (Intel and AMD 32-bit processors)
* Linux aarch (ARM 64-bit processors)
* Linux armhf (ARM 32-bit processors)
* Windows x64 (Intel and AMD 64-bits processors)
* Windows x86 (Intel and AMD 32-bit processors)

## Prebuilt binaries
You can find the prebuilt binaries on [this download page][mvdparser-builds].

## Prerequisites

None at the moment.

## Building binaries

### Build from source with CMake

Assuming you have installed essential build tools and ``CMake``
```bash
mkdir build && cmake -B build . && cmake --build build
```
Build artifacts would be inside ``build/`` directory, for unix like systems it would be ``mvdparser``.

You can also use ``build_cmake.sh`` script, it mostly suitable for cross compilation
and probably useless for experienced CMake user.
Some examples:
```
./build_cmake.sh linux-amd64
```
should build MVDPARSER for ``linux-amd64`` platform, release version, check [cross-cmake](tools/cross-cmake) directory for all platforms

```
B=Debug ./build_cmake.sh linux-amd64
```
should build MVDPARSER for linux-amd64 platform with debug

```
V=1 B=Debug ./build_cmake.sh linux-amd64
```
should build MVDPARSER for linux-amd64 platform with debug, verbose (useful if you need validate compiler flags)

```
G="Unix Makefiles" ./build_cmake.sh linux-amd64
```

force CMake generator to be unix makefiles

```
./build_cmake.sh linux-amd64
```

build MVDPARSER for ``linux-amd64`` version, you can provide
any platform combinations.

### Build from source with mingw32-make in cygwin

Install (for cygwin), 
mingw64-x86_64-gcc-core
mingw64-i686-gcc-core
cygwin-mingw

Navigate to Makefile in cygwin.
Use mingw32-make to make.

## Versioning

For the versions available, see the [tags on this repository][mvdparser-tags].

## Code of Conduct

We try to stick to our code of conduct when it comes to interaction around this project. See the [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) file for details.

## License

This project is licensed under the GPL-2.0 License - see the [LICENSE.md](LICENSE.md) file for details.

## Acknowledgments

* Thanks to the fine folks on [Quakeworld Discord][discord-qw] for their support and ideas.

[mvdparser]: https://github.com/QW-Group/mvdparser
[mvdparser-tags]: https://github.com/QW-Group/mvdparser/tags
[mvdparser-builds]: https://builds.quakeworld.nu/mvdparser
[discord-qw]: http://discord.quake.world/
