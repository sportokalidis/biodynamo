---
title: "Building Biodynamo"
date: "2019-01-01"
path: "/docs/devguide/build/"
meta_title: "BioDynaMo Dev Guide"
meta_description: "This is the how to build page."
toc: true
image: ""
next:
    url:  "/docs/devguide/build/"
    title: "Building Biodynamo"
    description: "This is the how to build page."
sidebar: "devguide"
keywords:
  -build
  -start
  -create
---

## C++ standard and ROOT packages

BioDynaMo's C++ standard is a property of the ROOT package it is built against,
not a fixed value. ROOT supports C++17, 20 and 23 from one source tree and
publishes different standards per platform and release, and its Cling
interpreter must parse the same headers the libraries were compiled from.
Configuration therefore reads `root-config` and adopts ROOT's language mode.
BioDynaMo currently builds and tests **C++17 and C++23**; another mode is
rejected with an actionable message rather than silently accepted.

In practice this means Ubuntu 22.04/24.04 and older Xcode versions keep building
in C++17 against today's packages, while platforms whose ROOT package is C++23,
such as Xcode 26 and Ubuntu 26.04 with a C++23 ROOT, build in C++23. Nothing
needs to be chosen by hand.

Requirements: CMake 3.24 or newer, ROOT 6.22 or newer, and ROOT 6.36 or newer
for a C++23 package, since C++23 interpreter support arrived in 6.36.

The compiler and ROOT must also report the same `__cplusplus` value, not merely
select the same standard. ROOT stores its build compiler's value in the public
`RConfigure.h` and warns from that header whenever a consumer disagrees, so
configuration rejects a mismatch instead of emitting that warning on every
translation unit. This matters mainly for C++23: GCC 13 reports the provisional
`202100L`, while GCC 14, Clang 17, and ROOT's own Cling report the final
`202302L`. For a C++23 build prefer **GCC 14 or newer, or Clang 17 or newer**,
with a ROOT package built by the same compiler. C++17 packages are unaffected,
since all supported compilers agree on `201703L`.

Use a fresh build directory after changing the compiler, standard, or ROOT, and
rebuild custom simulations and dictionaries as well. Simulations that pin
`set(CMAKE_CXX_STANDARD ...)` to something other than the standard BioDynaMo was
built with are rejected; remove the setting and let the use-file supply it.
`bdm-config --cxx-standard` reports the standard of an installed build.

### Selecting a C++23 ROOT package

Automatic downloads use the package registered for each platform in
`cmake/utils.cmake`. To test a package that is not registered yet, pin it
explicitly with a checksum:

```bash
cmake -S . -B build-cxx23 -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DBDM_ROOT_URL=https://root.cern/download/root_v6.40.04.Linux-ubuntu26.04-x86_64-gcc15.2.tar.gz \
  -DBDM_ROOT_SHA256=cd44b56ca02e00205b00bec11e207affda0fbbcc63dfd1e5bbd17286444ed4b9
```

That CERN package is C++23 and reports the final `__cplusplus` value, matching
Ubuntu 26.04's GCC 15.2. Note that using it currently stops at BioDynaMo's
Python pin: `CMakeLists.txt` requires exactly the pyenv Python the reused
Ubuntu 24.04 archives were built against, 3.9.25 on Ubuntu 26.04, whereas this
package's PyROOT is built for Ubuntu 26.04's Python 3.14.

Candidate packages are not interchangeable: ROOT 6.36.14 and 6.38.06 for Ubuntu
26.04 are C++17, as is 6.40.04 for Ubuntu 24.04. Check `include/RConfigure.h`
and `root-config --features` of any candidate rather than relying on its release
number. The [migration plan](cxx23_migration.md) and
[implementation log](cxx23_migration_log.md) record the remaining gates.

The ROOT packaging script accepts a pinned version and optional CMake flags:

```bash
# Install build dependencies first and select a compatible C++ compiler.
# Select Python to match the ParaView/ROOT ABI used by BioDynaMo.
BDM_ROOT_PYTHON=/path/to/python3.9 \
  util/build-third-party/build-root.sh 6.36.06
```

The script builds under `build/build-third-party/`, checks ROOT's language mode,
and produces an archive, SHA256 file, and build manifest. Set
`BDM_ROOT_SOURCE_DIR` to use local sources and `BDM_ROOT_WORK_DIR` to change the
workspace. It does not install system packages. Xcode 26.6 needs its newer
SDK-compatible ROOT release; use the version selected in `cmake/utils.cmake`.

## First build of BioDynaMo

To build BioDynaMo from source execute the following commands:

<h4><b>Note</b></h4>

If you are a new BioDynaMo user please follow the [installation](https://biodynamo.org/docs/userguide/installation/) instructions in our user guide.

```bash
git clone https://github.com/BioDynaMo/biodynamo.git
cd biodynamo

# Install the prerequisites
./prerequisites.sh all

# Create the build directory
mkdir build
cd build

# Build BioDynaMo
cmake ..
make

# (Optional) Installs the libraries
make install
```

The script `prerequisites.sh` is used to install all the dependencies needed by BioDynaMo. You will need
to run it once before actually calling `cmake`. It will choose the specific dependencies depending on the used operating system.
Run `./prerequisites.sh --help` to see how to use it.

<h4><b>Note</b></h4>

On macOS the prerequisites script will use `brew` to install the needed packages.
If you do not have `brew` on your system, do yourself a favour and install it.
Please have a look in the [prerequisites](https://biodynamo.org/docs/userguide/prerequisites/) page for more details.

## Ubuntu 26.04

Ubuntu 26.04 x86_64 support initially reuses the published Ubuntu 24.04 ROOT,
ParaView, Qt and Libroadrunner archives. The 26.04 checksum entries must match
those archives. This is a compatibility trial: run both Ubuntu CI workflows
before treating the platform as validated. SBML remains disabled in CI.

From a checkout containing this support:

```bash
./prerequisites.sh all
source util/installation/ubuntu-26.04/environment.sh
export PYENV_ROOT="$HOME/.pyenv"
export PATH="$PYENV_ROOT/bin:$PATH"
eval "$(pyenv init --path)"
eval "$(pyenv init -)"
pyenv shell "$BDM_PYTHON_VERSION"

cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release -Dsbml=OFF
cmake --build build --parallel
source build/bin/thisbdm.sh
cmake --build build --target run-unit-tests
```

BioDynaMo itself still builds with the platform's default compiler, same as on
Ubuntu 22.04/24.04; gcc-11 is installed (see prerequisites) only so ROOT's
Cling interpreter finds the matching headers it was built against. Python
3.9.25 retains the libraries' Python 3.9 ABI and supports the system OpenSSL 3.
Moving to a newer Python minor version requires rebuilding the third-party
libraries. CMake 3.31.10 is installed inside pyenv because bundled dependencies
still require compatibility removed in CMake 4. Always activate this
environment before configuring or rebuilding.

`./install.sh` activates the same environment automatically and installs to
`$HOME/biodynamo-v<version>`. Use a fresh build directory when changing Ubuntu
versions or compilers.

## Rebuilding BioDynaMo

If you make developments in the BioDynaMo code you will typically create a new branch and recompile after making your code changes:

```bash
cd biodynamo
git pull
git checkout -b <new-branch>

[edit the files]

# clean the previous build but keep the third party libraries, typically ROOT and ParaView
cd build
ninja cleanbuild

# Build BioDynaMo
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug ..
ninja
```

When developing `ninja` is preferred as it is slightly faster than `make`.

Also we advice you to put the following lines in your `.bashrc` or `.zshrc` file on Linux platforms where we use `pyenv` to manage the different python versions:

```bash
export PATH="$HOME/.pyenv/bin:$PATH"
eval "$(pyenv init -)"
``` 

Once finished, we hope that you want to contribute your code changes back to the BioDynaMo project. 
For more info on how to contribute with your code, please consult out guidelines from [here](https://github.com/BioDynaMo/biodynamo/blob/master/doc/dev_guide/contribute.md).

## CentOS 7 (deprecated)

For CentOS 7, you will need to run the following commands before actually calling `cmake`. 
This is because CentOS do not provide by default the correct C++ compilers and the correct python interpreter. 
Moerover, we will need to load the OpenMPI module. You will need to run these instructions only before building BioDynaMo. 
You will not need them in order to run the program.

```bash
export MESA_GL_VERSION_OVERRIDE=3.3
. scl_source enable devtoolset-10

. /etc/profile.d/modules.sh
module load mpi
```

## CMake Build Options

Our CMake build script uses a few options to influence the build process. 
They can be set as follows:

``` bash
cmake -Doption=value ..
```

The value for binary options is `on` or `off`.
If you change the value of these switches, you might have to delete `CMakeCache.txt` beforehand.

| Option     | Default Value | Description                                                                                                                    |
| ---------- | ------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| `test`     | `on`          | build the test executables; precondition for e.g. `valgrind` and `coverage`                                                    |
| `dict`     | `on`          | build ROOT dictionaries. These are compulsory to use backups. Turning them off reduces compilation time.                       |
| `paraview` | `on`          | Enable visualization using ParaView. Visualization cannot be used if this switch is turned off.                                |
| `libgit2`  | `off`         | Enable automatic git tracking for executed simulations (e.g. last commits and `git diff` outputs; not available on `CentOS`.). |
| `cuda`     | `off`         | enable CUDA code generation for GPU acceleration                                                                               |
| `opencl`   | `off`         | enable OpenCL code generation for GPU acceleration                                                                             |
| `valgrind` | `on`          | enable memory leak checks                                                                                                      |
| `coverage` | `off`         | creates a make target to generate a html report indicating which parts of the code are tested by automatic tests               |
| `jemalloc` | `off`         | use `jemalloc` for memory allocations                                                                                          |
| `tcmalloc` | `off`         | use `tcmalloc` for memory allocations                                                                                          |
| `website`  | `off`         | enable website generation (`make website<-live>` target (see below for more information))                                      |

### Further CMake command line parameters

| Option             | Description                                                                              |
| ------------------ | ---------------------------------------------------------------------------------------- |
| `CMAKE_CXX_FLAGS`  | specify additional compiler flags - e.g. `"-mavx"`                                       |
| `CMAKE_BUILD_TYPE` | specify the build type. Possible values are `Debug, Release, RelWithDebInfo, MinSizeRel` |

## Build Targets

| Target           | Description                                                                                                                                                                                                                                   |
| ---------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `run-unit-tests` | executes all BioDynaMo unit tests                                                                                                                                                                                                             |
| `run-valgrind`   | executes BioDynaMo valgrind tests                                                                                                                                                                                                             |
| `run-check`      | executes both unit and valgrind tests                                                                                                                                                                                                         |
| `run-demos`      | executes all demos and integration tests                                                                                                                                                                                                      |
| `clean`          | will clean all targets, also the external projects                                                                                                                                                                                            |
| `cleanbuild`     | will clean everything in the build directory, except for third_party (useful for avoiding downloading third party software)                                                                                                                   |
| `bdmclean`       | will only clean the `biodynamo` and `runBiodynamoTests*` targets                                                                                                                                                                              |
| `testbdmclean`   | will only clean the `runBiodynamoTests*` target                                                                                                                                                                                               |
| `doc`            | will generate the API, user and developer documentation in directory `build/doc`.                                                                                                                                                             |
| `coverage`       | will execute the test target and generate a coverage report in `build/coverage`. Make sure that `kcov` are installed and configure cmake with `cmake -Dcoverage=on ..`                                                                        |
| `coverage-build` | same as `make coverage`, but builds it in a separate directory (`build/coverage`). Since building the coverage report requires different compiler flags, building it in a separate directory keeps the current build directory in good order. |

### C++ Code Style Related Build Targets

The following targets are only available if `clang-format`, `clang-tidy` and `git` are installed.

Build targets indicated with `*` always come in three different flavors.

  * `no-suffix`: executes the target on source files that changed compared to origin/master -- e.g. `make check-format`
  * `-staged`: executes the target on source files that have been staged -- e.g. `make check-format-staged`
  * `-all`: executes the target on all source files in the project -- e.g. `make check-format-all`

| Target             | Description                                                                                                                                                                                                                                                  |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `check-format*`    | run clang-format on selected files. Fails if any file needs to be reformatted                                                                                                                                                                                |
| `show-format*`     | run clang-format on selected files and display differences                                                                                                                                                                                                   |
| `format*`          | run clang-format on selected files and update them in-place                                                                                                                                                                                                  |
| `check-tidy*`      | run clang-tidy on selected files. Fails if errors are found                                                                                                                                                                                                  |
| `show-tidy*`       | run clang-tidy on selected files and display errors.                                                                                                                                                                                                         |
| `tidy*`            | run clang-tidy on selected files and attempt to fix any warning automatically                                                                                                                                                                                |
| `check-cpplint*`   | run cpplint on selected files. Fails if errors are found and displays them.                                                                                                                                                                                  |
| `check-submission` | will build, run all tests, check formatting, code style, and generate documentation and coverage report                                                                                                                                                      |
| `fix-submission`   | will attempt to fix the reported issues using `clang-format` and `clang-tidy`. Failing build, tests, compiler warnings, issues from cpplint and warnings from doxygen must be fixed manually. Also some `clang-tidy` issues cannot be resolved automatically |

### Website Related Build Targets

| Target         | Description                                                                                                                                          |
| -------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| `website`      | will generate the static files used for biodynamo.org                                                                                                |
| `website-live` | starts a local web server so you can immediately view the website in the browser. The website is automatically reloaded if you change a source file. |

## Advanced Build Options

#### Use a Custom Compiler
If you need to user a custom compilers (instead of the one automatically detected by BioDynaMo) you will need to set the variables: `CXX` for the C++ compiler and `CC` for the C compiler. 
Your custom compiler must support C++23 and be compatible with OpenMP.
The complete procedure will become:

```bash
git clone https://github.com/BioDynaMo/biodynamo.git
cd biodynamo

# Let's say I want to use a custom version of clang
export CXX=/opt/local/bin/clang++-mp-8.0
export C=/opt/local/bin/clang++-mp-8.0

./install.sh
```

#### Use a Specific ROOT / ParaView Installation

When you want to inform BioDynaMo of a specific installation of ROOT and /or ParaView on your system,
you will need to perform the following instructions prior to installation.

```bash
git clone https://github.com/BioDynaMo/biodynamo.git
cd biodynamo

# For ROOT
source <root_installation_dir>/bin/thisroot.sh

# For ParaView
export ParaView_DIR=<paraview_installation_dir>/lib/cmake/paraview-5.8
export Qt5_DIR=<qt5_installation_dir>/lib/cmake/Qt5

./install.sh
```

<h4><b>Note</b></h4>

If you specify `ParaView_DIR`, then you will need to provide also the `Qt5_DIR` variable as well.
This is because ParaView implicitly relies on the Qt5 installation.


#### Speed Up Installation Tests with a Local BioDynaMo-LFS Copy

The installation scripts fetch large precompiled dependencies like ROOT or ParaView
from biodynamo's large file storage (LFS). To enable faster builds you can download the whole
LFS and tell BioDynaMo to access the local version instead. This is done with the
environmental flag `BDM_LOCAL_LFS`. Use an absolute path to the directory
that contains the local copy.

``` bash
export BDM_LOCAL_LFS=/path/to/local/lfs
```

If you want to download the files from remote LFS again execute:

``` bash
unset BDM_LOCAL_LFS
```


<h4><b>Warning</b><h4>

At the moment there is no check if the local copy is in synch with remote. 
You have to ensure that yourself!
