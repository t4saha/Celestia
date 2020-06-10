# Basic installation instructions

Stable version installation on Unix-like systems (e.g. GNU/Linux or *BSD):
* Check your OS repository for already built packages.
* Check https://celestia.space/download.html.
* Check https://bintray.com/celestia if it contains packages for your system.

Stable version installation on Windows and OSX:
* Check https://celestia.space/download.html.

Development snapshots installation on Unix-like systems:
### On Debian 10 and derived systems:

```
curl https://download.opensuse.org/repositories/home:/munix9:/unstable/Debian_10/Release.key | sudo apt-key add -
echo "deb https://download.opensuse.org/repositories/home:/munix9:/unstable/Debian_10/ ./" | sudo tee /etc/apt/sources.list.d/celestia-obs.list
sudo apt update && sudo apt install celestia
```

### On Ubuntu 18.04/20.04 and derived systems:

```
curl https://download.opensuse.org/repositories/home:/munix9:/unstable/Ubuntu_${VERSION}/Release.key | sudo apt-key add -
echo "deb https://download.opensuse.org/repositories/home:/munix9:/unstable/Ubuntu_${VERSION}/ ./" | sudo tee /etc/apt/sources.list.d/celestia-obs.list
sudo apt update && sudo apt install celestia
```

Where VERSION is 18.04 or 20.04.


### On openSUSE Leap 15.1:

```
sudo zypper addrepo https://download.opensuse.org/repositories/home:munix9:unstable/openSUSE_Leap_15.1/home:munix9:unstable.repo
sudo zypper refresh
sudo zypper install celestia
```

### On openSUSE Leap 15.2:

```
sudo zypper addrepo https://download.opensuse.org/repositories/home:munix9:unstable/openSUSE_Leap_15.2/home:munix9:unstable.repo
sudo zypper refresh
sudo zypper install celestia
```

### On openSUSE Tumbleweed:

```
sudo zypper addrepo https://download.opensuse.org/repositories/home:munix9:unstable/openSUSE_Tumbleweed/home:munix9:unstable.repo
sudo zypper refresh
sudo zypper install celestia
```

### On other GNU/Linux distributions:

  Try experimental portable AppImage (see https://github.com/CelestiaProject/Celestia/issues/333):
```
wget https://download.opensuse.org/repositories/home:/munix9:/unstable/AppImage/celestia-1.7.0-git-x86_64.AppImage
chmod 755 celestia-1.7.0-git-x86_64.AppImage
```

  Optionally create a portable, main version-independent `$HOME` directory in the same folder as the AppImage file:
```
mkdir celestia-1.7.home
```

Development snapshots installation on Windows:

* https://bintray.com/celestia/celestia-builds/snapshots contains
  official 32/64 bit development snapshots for Windows.


To build from sources please follow instructions below.



## Common building instructions

We recommend using a copy of our git repository to build your own installation
as it contains some dependencies required for building.

To create the copy install git from your OS distribution repository or from
https://git-scm.com/ and then execute the following commands:

```
git clone https://github.com/CelestiaProject/Celestia
cd Celestia
git submodule update --init
```

If you have fmtlib and Eigen3 installed from other sources (OS repository or
vcpkg) or don't want to build Celestia with SPICE support then you can skip the 3rd step.


## Celestia Install instructions for UNIX

First you need a C++ compiler able to compile C++11 code (GCC 4.8.1 or later,
Clang 3.3 or later), CMake, GNU Make or Ninja.

Then you need to have the following devel components installed before Celestia
will build: OpenGL, glu, libepoxy, theora, libjpeg, and libpng. Optional
packages are fmtlib, Eigen3, Qt5, Gtk2 and glut.

For example on modern Debian-derived system you need to install the following
packages: libepoxy-dev, libjpeg-dev, libpng-dev, libtheora-dev, libgl1-mesa-dev,
libglu1-mesa-dev. Then you may want to install libeigen3-dev, libfmt-dev;
qtbase5-dev, qtbase5-dev-tools and libqt5opengl5-dev if you want to build with
Qt5 interface; libgtk2.0-dev and libgtkglext1-dev to build with legacy Gtk2
interface; or freeglut3-dev to build with glut interface.

OK, assuming you've collected all the necessary libraries, here's
what you need to do to build and run Celestia:

```
mkdir build
cd build
cmake .. -DENABLE_INTERFACE=ON [*]
make
sudo make install
```

[*] `INTERFACE` must be replaced with one of "`QT`", "`GTK`", "`SDL`" or
"`GLUT`".

Four interfaces are available for Celestia on Unix-like systems:
- GLUT: minimal interface, barebone Celestia core with no toolbar or menu...
       Disabled by default.
- SDL: minimal interface, barebone Celestia core with no toolbar or menu...
       Disabled by default.
- GTK: A full interface with minimal dependencies, adds a menu, a configuration
       dialog some other utilities. Legacy interface, may lack some new
       features. Disabled by default.
- QT: A full interface with minimal dependencies, adds a menu, a configuration
      dialog some other utilities, bookmarks... A preferred option. Enabled by
      default, No need to pass -DENABLE_QT=ON.

Starting with version 1.3.1, Lua is the new scripting engine for Celestia,
the old homegrown scripting engine is still available. By default Lua support
is enabled, it can be disabled passing -DENABLE_CELX=OFF to cmake.
Versions 5.1, 5.2 or 5.3 of Lua library is required. On Debian-based systems
either one of liblua5.3-dev, liblua5.2-dev or liblua5.1-dev should be installed
to have Lua support.

To check wether your Celestia has been compiled with Lua support, go to File
-> Open. If you have '*.cel *.celx' in the filter box, then Lua is available
otherwise the filter will contain only '*.cel'.

The GtkGLExt widget that is required in order to build Celestia with Gtk+ may
be downloaded from http://gtkglext.sf.net. Note that depending in your
distribution you may also need other packages containing various files needed
by the build process. For instance, to build under SUSE Linux, you will also
need to have the gtk-devel package installed.

Celestia will be installed into /usr/local by default, with data files landing
in /usr/local/share/celestia, but you may specify a new location with the
following option to cmake: -DCMAKE_INSTALL_PREFIX=/another/path.


## Celestia Install instructions for Windows (MSVC)

Currently to build on Windows you need a Visual Studio 2015 or later, CMake
and vcpkg (*).

Install required packages:

```
vcpkg install libpng libjpeg-turbo gettext lua fmt libepoxy eigen3
```

Install optional packages:

```
vcpkg install qt5 luajit
```

Configure and build 32-bit version:

```
md build32
cd build32
cmake -DCMAKE_TOOLCHAIN_FILE=c:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake ..
cmake --build . --  /maxcpucount:N /nologo
```

Configure and build 64-bit version:

```
md build64
cd build64
cmake -DCMAKE_GENERATOR_PLATFORM=x64 -DCMAKE_TOOLCHAIN_FILE=c:/tools/vcpkg/scripts/buildsystems/vcpkg.cmake ..
cmake --build . --  /maxcpucount:N /nologo
```

Instead of N in /maxcpucount pass a number of CPU cores you want to use during
a build.

If you have Qt5 installed using official Qt installer, then pass parameter
CMAKE_PREFIX_PATH to cmake call used to configure Celestia, e.g.

```
cmake -DCMAKE_PREFIX_PATH=C:\Qt\5.10.1\msvc2015 ..
```

Not supported yet:
- automatic installation using cmake
- using Ninja instead of MSBuild

Notes:
 * vcpkg installation instructions are located on
   https://github.com/Microsoft/vcpkg


## Celestia Install instructions for Windows (MINGW64), qt-only

It is recommended to build the source with MSYS2
https://www.msys2.org/ .

Do the following in the MINGW64 shell (mingw64.exe).

Install required packages:

```
pacman -S mingw-w64-x86_64-toolchain
pacman -S base-devel
pacman -S git
pacman -S mingw-w64-x86_64-cmake
pacman -S mingw-w64-x86_64-qt5
pacman -S mingw-w64-x86_64-freeglut mingw-w64-x86_64-libepoxy mingw-w64-x86_64-lua
pacman -S mingw-w64-x86_64-libtheora mingw-w64-x86_64-mesa
```

Install optional packages:

```
pacman -S mingw-w64-x86_64-fmt mingw-w64-x86_64-eigen3 mingw-w64-x86_64-luajit
pacman -S mingw-w64-x86_64-sld2
```

Clone the source and go to the source directory.

Configure and build:

```
mkdir build
cd build
cmake .. -G"MSYS Makefiles" -DENABLE_WIN=OFF
mingw32-make.exe -jN
```

Instead of N, pass a number of CPU cores you want to use during a build.

To build in debug configuration, you have to use lld linker instead of the
default linker in gcc.

```
pacman -S mingw-w64-x86_64-lld mingw-w64-x86_64-lldb
```

Follow by:

```
cmake .. -G "MSYS Makefiles" -DENABLE_WIN=OFF -DCMAKE_CXX_FLAGS='-fuse-ld=lld' -DCMAKE_BUILD_TYPE=Debug
```

Then do `mingw32-make.exe`.

## Celestia Install instructions for macOS, qt-only

Currently Qt frontend is the only available option for macOS users building
Celestia from source.

Install the latest Xcode:

  You should be able to get Xcode from the Mac App Store.

Install Homebrew

  Follow the instructions on https://brew.sh/

Install required packages:

```
brew install cmake fmt gettext libepoxy libpng lua qt5 jpeg eigen
```

Install optional packages:

```
brew install cspice
```

Follow common building instructions to fetch the source.

Configure and build:

```
mkdir build
cd build
cmake ..
make -jN
```

Instead of N, pass a number of CPU cores you want to use during a build.

Install:

```
make install
```

Celestia will be installed into /usr/local by default, with data files landing
in /usr/local/share/celestia, but you may want to specify a new location with
the following option to cmake: `-DCMAKE_INSTALL_PREFIX=/another/path`.

To build the application bundle, pass -DNATIVE_OSX_APP=ON to the cmake command,
the application bundle will be located in the "build" folder that you previously
created.

## Supported CMake parameters

List of supported parameters (passed as `-DPARAMETER=VALUE`):

 Parameter            | TYPE | Default | Description
----------------------| ------|---------|--------------------------------------
| CMAKE_INSTALL_PREFIX | path | *       | Prefix where to install Celestia
| CMAKE_PREFIX_PATH    | path |         | Additional path to look for libraries
| LEGACY_OPENGL_LIBS   | bool | \*\*OFF   | Use OpenGL libraries not GLvnd
| ENABLE_CELX          | bool | ON      | Enable Lua scripting support
| ENABLE_SPICE         | bool | OFF     | Enable NAIF kernels support
| ENABLE_NLS           | bool | ON      | Enable interface translation
| ENABLE_GLUT          | bool | OFF     | Build simple Glut frontend
| ENABLE_GTK           | bool | \*\*OFF   | Build legacy GTK2 frontend
| ENABLE_QT            | bool | ON      | Build Qt frontend
| ENABLE_SDL           | bool | OFF     | Build SQL frontend
| ENABLE_WIN           | bool | \*\*\*ON   | Build Windows native frontend
| ENABLE_THEORA        | bool | \*\*ON    | Support video capture to OGG Theora
| ENABLE_TOOLS         | bool | OFF     | Build tools for Celestia data files
| ENABLE_TTF           | bool | \*\*\*\*OFF | Build with FreeType support
| ENABLE_DATA          | bool | OFF     | Use CelestiaContent submodule for data
| ENABLE_GLES          | bool | OFF     | Use OpenGL ES 2.0 in rendering code
| NATIVE_OSX_APP       | bool | OFF     | Support native OSX data paths

Notes:
 \* /usr/local on Unix-like systems, c:\Program Files or c:\Program Files (x86)
   on Windows depending on OS type (32 or 64 bit) and build configuration.
   This option effect is overriden by NATIVE_OSX_APP.
 \*\* Ignored on Windows systems.
 \*\*\* Ignored on Unix-like systems.
 \*\*\*\* This option support is not finished yet.

Parameters of type "bool" accept ON or OFF value. Parameters of type "path"
accept any directory.

On Windows systems two additonal options are supported:
- `CMAKE_GENERATOR_PLATFORM` - can be set to x64 on 64-bit Windows to build
  64-bit Celestia. To build 32-bit Celestia it should be omitted.
- `CMAKE_TOOLCHAIN_FILE` - location of vcpkg.cmake if vcpkg is used.


Executable files
----------------

As said prevously Celestia provides several user interfaces, accordingly with
interfaces it's built with it has different executable files installed to
${CMAKE_INSTALL_PREFIX}/bin (e.g. with default CMAKE_INSTALL_PREFIX on
Unix-like systems they are installed into `/usr/local/bin`).

Here's the table which provides executable file names accordingly to interface:

 Interface  | Executable name
|-----------|----------------|
| Qt5       | celestia-qt
| GTK       | celestia-gtk
| GLUT      | celestia-glut
| SDL       | celestia-sdl
| WIN       | celestia-win
