---
title: "C++23 migration implementation log"
date: "2026-10-16"
path: "/docs/devguide/cxx23-migration-log/"
meta_title: "BioDynaMo C++23 implementation log"
meta_description: "Changes, rationale, validation, and remaining migration gates."
toc: true
sidebar: "devguide"
---

# C++23 migration implementation log

Branch: `feature/cxx23-derived-standard`; starting revision: `ab578242` (`master`).
Earlier entries were recorded while the branch pinned C++23; the
2026-10-20 policy entry below explains why that was inverted.
The [migration plan](cxx23_migration.md) describes the intended rollout.

## 2026-10-15 — Baseline and implementation approach

- Inspected tracked build settings, ROOT discovery/download paths, dictionary
  generation, consumers, external builds, installers, and workflows.
- Ran the existing `build/bin/biodynamo-unit-tests` through `build/launcher.sh`:
  **438 passed, four disabled**, in approximately 35 seconds. This is an existing
  C++17 binary, not a clean rebuild of this revision. Log:
  `/tmp/bdm-cxx17-baseline.log`.
- Local tools: GCC 13.3.0; default CMake 3.19.3; usable alternative CMake 3.27.9
  at `/opt/cmake/bin/cmake`; system CMake 4.2.0. Existing ROOT is 6.30.02/C++17.
- Implement the final C++23 policy on this unmerged branch. Preserve historical
  package metadata, but never download a known C++17 package for this build.
  Linux and older Xcode distribution remain a release gate until actual C++23
  artifacts exist. No replacement checksums have been invented.

## Language policy and ROOT discovery

- Added `cmake/CompilerStandard.cmake`, included before dependency discovery and
  by installed consumer setup. Require CMake 3.24+, the compiler's C++23 mode,
  required standard 23, and no extensions. Reject conflicting consumer settings
  and raw standard flags so custom commands cannot use a different mode.
- Updated demos, template, and system-test CMake minimums and explicit language
  settings; updated active CMake bootstrap pins to 3.31.10. Old unsupported
  Ubuntu 18.04/CentOS scripts remain historical, not newly supported platforms.
- Added ROOT standard normalization and diagnostics. Compare flags and feature
  metadata, recognize draft aliases, reject ambiguity, and check subprocess
  failures. Discovery now confines libraries and dictionary executables to the
  selected ROOT installation and clears stale cached selections.
- Check language compatibility after every resolution path, including downloads.
  Use exact canonical paths when deciding whether an installation is managed;
  a similarly named external directory must never be removed as a cache entry.
- Added checksum-pinned `BDM_ROOT_URL`/`BDM_ROOT_SHA256` overrides so newly built
  archives can be exercised before publication. Built-in automatic downloads
  are restricted to the existing Xcode 26 C++23 packages. Refuse unavailable
  replacements before removing a cache.

## Consumers and external builds

- Propagate C++23 through library compile features and validate the policy when
  BioDynaMo helper functions create consumer targets.
- Resolve dictionary tools from the validated ROOT installation, whose built-in
  language mode must be C++23. Expose `bdm-config --cxx-standard` for consumers
  and use genreflex from root-config's prefix.
- Pass the selected compiler/standard to GoogleTest, Google Benchmark, and
  OptimLib. OptimLib's generated Makefile hardcodes C++11, so override its
  `CXX_STD` make variable instead of appending another conflicting flag.

## 2026-10-16 — Dependency and source validation

- The first full configure hit an OpenMPI discovery issue inside the sandbox.
  Moved ROOT resolution ahead of unrelated dependency discovery; a subsequent
  fresh configure correctly rejects the installed ROOT 6.30.02/C++17 with an
  actionable diagnostic. It does not change that external installation.
- **22 configuration regression tests pass** with CMake 3.27.9. These cover
  supported/draft standard spellings, missing/conflicting metadata, failed
  queries, missing components/tools, stale discoveries, cache replacement,
  repeat verification, corrupt archives, offline mirrors, and consumer flags.
  Fake ROOT packages exercise configuration only, not ROOT runtime behavior.
- Fixed the offline `BDM_LOCAL_LFS` path: it previously copied an archive without
  extracting or verifying it. Local and remote archives now share checksum
  verification/extraction, and local source archives are preserved.
- **62 translation units passed GCC 13.3 C++23 syntax checks** against the ROOT
  6.36.06 source build's real headers/configuration: all 61 core/neuroscience
  source entries in the existing CPU compilation database plus the new runtime
  standard test. The command adaptation replaces only the compiler wrapper,
  standard, ROOT include paths, and output mode. This is compile-only evidence;
  no link or runtime success is implied. Logs are in
  `build-cxx23-validation/source-syntax*.log`.
- Common vendored headers (cxxopts, JSON, cpptoml, nanoflann) compiled and ran in
  standalone C++23 probes with GCC 13.3 and Clang 18, both using libstdc++.
  libc++ is not installed locally, so that library combination is unvalidated.
- GoogleTest 1.17.0 and Google Benchmark 1.5.5 build successfully with C++23.
  OptimLib also builds successfully with C++23, reusing Armadillo 11.4.4 from the
  existing dependency tree after its configure script attempted an unavailable
  network clone. No vendored source upgrade was needed for these checks.
- Downloaded official ROOT 6.36.06 sources and started a C++23 build in
  `build-cxx23-root`, with Python 3.9.1, X11, image support, OpenGL, and TBB/IMT.
  The source build is still compiling bundled LLVM/Cling. The first minimal
  configuration lacked X11/IMT; corrected it before continuing. ROOT requires
  `asimage=ON` when enabling OpenGL. No finished archive/checksum is claimed yet.
- Added a unit test that checks Cling is in C++23 mode and agrees with compiled
  code on `sizeof(bdm::Cell)`. It passes syntax checking and awaits the linked
  C++23 runtime suite.
- Added CI configuration checks for CMake 3.24.3 and 3.31.10 on Linux/macOS,
  plus full demo/system coverage in the existing macOS 26 build. Updated Ubuntu
  style checks to use distribution clang-tidy 18 on Ubuntu 24.04 instead of the
  old LLVM 10 repository. These workflows have not been run on GitHub.
- Updated user/build documentation and unreleased migration notes. Existing
  Linux/older-Xcode CI remains a visible release gate until matching ROOT
  artifacts are available; no jobs were disabled to conceal that limitation.
- Shell syntax, workflow YAML parsing, the existing Ubuntu platform-selection
  test, and formatting/cpplint for the new C++ test pass.

- The separate ParaView 5.9 plugin configured, compiled, linked, and passed a
  headless runtime smoke test: loaded it in `pvpython`, created a `BDMGlyph`
  filter, and verified nonempty output geometry. Explicit Python 3.9 library
  paths were needed; MPI required execution outside the socket-restricted
  sandbox. Full BioDynaMo Catalyst integration still awaits ROOT.
- Review of ROOT 6.36.06's actual option parser found that `rootcling` and
  `genreflex` do not accept a compiler `-std=` flag: it collides with their `-s`
  option. Removed the initially added flags and instead revalidate ROOT's
  built-in mode at dictionary command creation. The existing ROOT tool also
  reproduced this CLI parsing issue. This avoids a predictable dictionary
  failure before the new ROOT build finishes.
- Makefile and OptimLib compiler flags now use CMake's selected C++23 spelling,
  preserving support for compilers which require `-std=c++2b`.
- CUDA toolkit and OpenCL C headers are absent locally. A standalone OpenCL
  header probe stops at missing `CL/opencl.h`; it is not evidence of a C++23
  source incompatibility. Device runtime validation remains pending.

## 2026-10-19 — Clean build and runtime validation

- All **69 unit-test translation units** also passed the C++23 syntax check,
  including the new interpreter test. Results are in
  `build-cxx23-validation/unit-syntax*.log`.
- ROOT 6.36.06 finished building and installing. Its installed `root-config`
  reports `-std=c++23` and `cxx23`; the interpreter runs and reports
  `__cplusplus=202302`. The packaging tail was interrupted because the running
  shell script had been edited during the earlier build. Restarted the recipe
  to finish verification and packaging; no archive is claimed until it finishes.
- A fresh Debug configuration in `build-cxx23`, with dictionaries and tests
  enabled and ParaView disabled, accepts the installed C++23 ROOT. The first
  build hit missing Armadillo headers after OptimLib's download failed. Supplied
  the same cached Armadillo 11.4.4 headers used in the dependency probe and
  resumed; this is a local dependency workaround, not a vendored-source change.
- The 22 configuration regressions pass with GCC 13 and CMake 3.24.3, 3.27.9,
  and 3.31.10, and with Clang 18/CMake 4.2.0. Actual dependency installation
  exposed a CI pin error: PyPI does not provide CMake 3.24.4. Corrected the
  minimum-version job to the available 3.24.3 and tested both CI versions in
  isolated `/tmp` installations.
- All 136 entries in the fresh BioDynaMo compilation database contain exactly
  one `-std=c++23` flag. ROOT dictionary generation succeeds. ROOT's own header
  still warns during interpretation: this ROOT build carries the provisional
  C++23 macro value `202100L` from its GCC 13 build compiler, while Cling
  reports `202302L`. Both select C++23; the warning is retained, and runtime
  validation is needed before declaring this compiler/interpreter combination
  supported. The 2026-10-20 entry below corrects an earlier reading of this
  warning and records the resulting checks.
- Explicit `BDM_ROOT_URL`/`BDM_ROOT_SHA256` selection now takes precedence over
  an older ROOT in the caller's environment. This allows testing replacement
  packages without first removing or changing that external installation.
- Updated the Ubuntu prerequisite commands to install the same CMake 3.31.10
  used by the installer scripts; moved branch requirements to the top of the
  page so Linux users see them too.
- Added the ROOT version-floor regression (including legacy slash-formatted
  versions): **23 configuration tests pass** with minimum CMake 3.24.3. The
  installed CMake 3.19.3 correctly refuses the main project before configuration.
  Ubuntu dependency cache keys now include `cxx23` and the package-selection
  implementation, preventing reuse of the C++17 cache namespace.
- Found a second environment-selection issue: the build launcher inherited a
  caller's previously sourced ROOT, even after CMake selected a different one.
  It now pins the configured ROOT prefix, stops if environment setup fails, and
  preserves command argument boundaries. A regression with stale ROOT variables,
  paths/arguments containing spaces, and failed setup brings the configuration
  suite to **24 passing tests** on minimum CMake 3.24.3.
- Created a fresh simulation with `biodynamo new` outside the repository, built
  it with C++23 dictionaries, ran it successfully, and passed both generated
  CTest tests. Log: `/tmp/bdm-cxx23-template.log`.
- Built a small backup probe with the existing C++17 BioDynaMo/ROOT 6.30.02 and
  wrote a simulation containing a cell and 26 completed steps. A separately
  compiled C++23 probe using the new library/ROOT 6.36.06 restored it and verified
  the step count, agent count, diameter, and position. This validates that fixture,
  not every historical backup or custom-agent schema. Source, binaries, and
  fixture are in `/tmp/bdm-cxx23-consumers`; logs are
  `/tmp/bdm-cxx23-legacy-backup.log` and `/tmp/bdm-cxx23-restore.log`.
- The fresh **Debug C++23 build passed all 439 unit tests**, with four existing
  disabled tests, in approximately 66 seconds using two OpenMP threads.
  This includes the new Cling/public-header agreement test, JIT/reflection,
  serialization, diffusion, and neuroscience coverage. Log:
  `/tmp/bdm-cxx23-unit-tests.log`.
- The existing Makefile-project system test also passes, including dictionary
  generation, compilation, linking, and simulation execution. Log:
  `/tmp/bdm-cxx23-makefile.log`.
- Verified the regenerated build launcher against actual stale C++17 ROOT
  environment variables: it still selects the configured C++23 installation.
- Started a separate Release build with ParaView 5.9 enabled, reusing the local
  ParaView/Qt packages and matching Python 3.9.1. CMake warns about overlapping
  TBB library search paths; integration runtime tests will check this combination.
- The crash/restart backup system test passes with one OpenMP thread. This
  exercises an actual interrupted simulation and recovery, separately from the
  legacy-file probe. Log: `/tmp/bdm-cxx23-backup-system.log`.
- PyROOT can load BioDynaMo through its generated `etc/rootlogon.C`, parse public
  headers, and construct/use a cell in C++23 mode. The initial bare library-load
  probe lacked BioDynaMo's include paths; using the supported setup macro fixes
  that harness issue. Log: `/tmp/bdm-cxx23-pyroot.log`.
- Testing `dict=OFF`, `real_t=float` exposed a pre-existing install-rule bug:
  it referenced the dictionary-only `biodynamo-unit-tests-bin` target. Corrected
  installation to use the standalone executable when dictionaries are off.
  The new interpreter test always checks C++23, but checks dictionary-provided
  public headers only when dictionaries are enabled. The separate float/no-dict
  build is now in progress.

## 2026-10-20 — Package validation and optional-build fixes

- The ROOT recipe completed verification and packaging. Local artifact:
  `build-cxx23-root/root_v6.36.06_cxx23_python3.9_ubuntu-24.04-x86_64.tar.gz`
  (183 MiB), SHA256:
  `fab47a92ba39e6a4c392b8a3203af84ad672a5ddb2632a7ace051412acc09b0c`.
  Its build manifest is inside the archive. This is a locally tested artifact,
  not a published replacement for the supported-platform download matrix.
- Configured a fresh BioDynaMo build using that real archive and checksum while
  explicitly pointing both the environment and cached root-config at old C++17
  ROOT. The override correctly selected/extracted C++23 ROOT. A second configure
  retained a sentinel in the package cache. The extracted interpreter runs from
  its new prefix and reports `__cplusplus=202302`. Logs:
  `/tmp/bdm-cxx23-package-{check,reconfigure,runtime}.log`.
- The no-dictionary build exposed an unconditional `ClassDefNV` in
  `experimental::Style`, leaving undefined streamer/class symbols at link time.
  Replaced it with BioDynaMo's existing `BDM_CLASS_DEF_NV` wrapper and included
  its defining header. Dictionary-enabled expansion and serialized fields remain
  the same; dictionary-disabled builds now receive the existing stub behavior.
- The ParaView help-generator crash is an environment issue: gdb identifies
  Qt 5.12's `QXdgDesktopPortalTheme::palette`. Inherited Snap application markers
  select that plugin. Removing `SNAP` and `SNAP_NAME` only for the build process
  makes the same help generator run successfully. Resumed the Release build
  with that local environment adjustment and offscreen Qt; no Qt sources or
  plugin documentation targets were disabled. Backtrace:
  `/tmp/bdm-cxx23-qt-backtrace.log`.
- The ParaView plugin now builds through the parent Release build, including
  its Qt help file. The full demo/system suite is running against that build.
- A fixed-seed mechanical simulation (32 cells, 20 steps, one OpenMP thread)
  produced identical positions and diameters with the baseline C++17 Release
  library and C++23 Debug library: maximum absolute difference was zero across
  128 reported values. This is one numerical regression probe, not a performance
  benchmark or a claim about every model. Source and results are under
  `/tmp/bdm-cxx23-consumers` and `/tmp/bdm-cxx{17,23}-numerical-results.log`.
- Bash and fish environment setup now apply `bdm-config`'s ROOT language check
  after selecting ROOT, so users cannot silently source an incompatible ROOT
  and then run existing C++23 binaries. Fish also preserves the failure status
  after cleanup. Both shell syntax checks pass; runtime validation follows below.
- All 24 configuration tests also pass with the installer/CI CMake 3.31.10.
- The separate Debug build with `dict=OFF`, `real_t=float`, and ParaView off
  now compiles and links successfully. All 61 tests selected by
  `RootStandardTest.*:CellTest.*:MathArrayTest.*:DiffusionTest.*:MechanicalInteraction.*:NeuronSomaTest.*`
  pass (five matching suites). This is deliberately a feature-appropriate subset:
  existing JSON parameter and time-series serialization tests require dictionaries.
  It is not a full-suite result for dictionary-disabled builds. Logs:
  `/tmp/bdm-cxx23-nodict-resume.log` and `/tmp/bdm-cxx23-nodict-tests.log`.
- Runtime checks of the generated bash and fish setup scripts both accept the
  real C++23 ROOT installation and return status 1 with an actionable diagnostic
  for the old C++17 installation. Fish configuration writes were isolated under
  temporary `XDG_CONFIG_HOME` directories. Existing fish `type -qt` diagnostics
  are unrelated to the language check and remain unchanged. Log:
  `/tmp/bdm-cxx23-environment-tests.log`.

- The Release build with ParaView 5.9 and dictionaries enabled passed **all 458
  unit tests** in 83 suites, with five existing disabled tests. The four suites
  beyond the Debug run are exactly the visualization ones enabled by ParaView:
  `ParaviewAdaptorTest`, `GetDataMemberForVisTest`, `VisualizationAdaptorDeathTest`,
  and `FLAKY_ParaviewIntegrationTest`. Log: `/tmp/bdm-cxx23-release-unit-tests.log`.
- The **full demo and system suite passed against that Release build**: all 19
  runnable demos, the generated simulation, the Makefile project, crash/restart
  backup/restore, multiple simulations, and substance initialization. Twelve
  tests produced and verified ParaView state output. The SBML test skipped
  itself correctly because this build has no libroadrunner. No `error:`
  diagnostics occurred. The remaining warnings are pre-existing and not caused
  by the language mode: deprecated `std::iterator` uses inside Boost uBLAS
  reached through odeint, `-Winterference-size` from
  `core/container/shared_data.h`, and existing overloaded-virtual and
  unused-variable warnings. Log: `/tmp/bdm-cxx23-system-tests.log`.
- Installing the Release build into a temporary prefix and running its tests
  from outside the source tree passed, including the Cling agreement test.
  Logs: `/tmp/bdm-cxx23-install.log` and `/tmp/bdm-cxx23-installed-consumer.log`.
- A fixed-seed probe against the C++23 **Release** library reproduced the same
  positions and diameters as the C++17 baseline, matching the earlier Debug
  comparison. Log: `/tmp/bdm-cxx23-release-numerical-results.log`.
- clang-tidy 18 parses the C++23 compilation database. It reports only two
  pre-existing `experimental::Style` findings, unrelated to the language mode.
  Log: `/tmp/bdm-cxx23-clang-tidy.log`.

## 2026-10-20 — `__cplusplus` agreement between ROOT, the compiler, and Cling

- Diagnosed the ROOT header warning recorded on 2026-10-19 more precisely; the
  earlier explanation had the direction reversed. ROOT writes its *build*
  compiler's `__cplusplus` into the installed public header as
  `#define ROOT__cplusplus`, and `RConfigure.h` warns whenever a consumer's
  value differs. The locally built package was produced by GCC 13 and therefore
  carries the provisional `202100L`, while its Cling reports the final `202302L`:
  - GCC 13 compiling against it: silent, because both are `202100L`.
  - Cling interpreting it: warns, and so does every dictionary generation.
  - Clang 18 compiling against it: warns on every translation unit; verified
    directly against the installed headers.
  Consequently that archive suits GCC 13 only. This matters for the migration's
  motivating platform: Ubuntu 26.04's GCC reports `202302L` and would mismatch
  such a package in every user simulation.
- The standard check alone cannot catch this, since both sides correctly report
  C++23. Added `bdm_root_cplusplus_from_header`, `bdm_compiler_cplusplus`, and
  `bdm_check_cplusplus_agreement`, called from `verify_ROOT()` once the C++
  language is enabled. A differing value is now a configure-time error naming
  both values, because the macro also selects preprocessor branches in ROOT's
  headers and can expose declarations its libraries were not built with. Equal
  but provisional values configure successfully with a warning that the
  interpreter still disagrees, so GCC 13 remains usable as a probe without
  being presented as a supported toolchain.
- Verified both paths against the real installation: GCC 13 configures with the
  provisional warning, and Clang 18 is rejected with both values reported.
- The packaging recipe now refuses to label an archive whose `ROOT__cplusplus`
  is not `202302L`, and records the value in the build manifest. Applied to the
  existing local artifact, this check correctly rejects it. Published packages
  must be built with GCC 14+ or Clang 17+ so that compiled code, ROOT's headers,
  and Cling all agree and no warning remains.
- Six configuration regressions cover header parsing, a duplicated macro, a
  missing `RConfigure.h`, agreeing and disagreeing values, the probe's
  agreement with a direct preprocessor run, and the download path with a C++
  compiler enabled: **30 configuration tests pass** with CMake 3.27.9. The
  download coverage matters because that path resolves ROOT inside
  `verify_ROOT()`, so the check queries `root-config --incdir` rather than
  reading a discovery variable set in another scope.
- The Debug build rebuilt cleanly and still passes all 439 unit tests after
  these changes. Log: `/tmp/bdm-cxx23-recheck-tests.log`.
- `show-format`, `check-cpplint`, and clang-format/cpplint on the new test file
  pass, as does a byte-compile of the configuration test harness.

## 2026-10-20 — Prebuilt package survey

- Checked whether a published package can replace the local source build.
  Confirmed that Linux currently resolves `root_v6.30.02_cxx17_python3.9_*` for
  every Ubuntu release, and that `bdm_third_party_os()` maps Ubuntu 26.04 to the
  Ubuntu 24.04 package, so 26.04 presently receives ROOT 6.30.02 in C++17.
- Inspected CERN's published Linux binaries by streaming each archive and
  reading its own `include/RConfigure.h` and `bin/root-config`:
  `root_v6.40.04.Linux-ubuntu26.04-x86_64-gcc15.2.tar.gz` reports
  `ROOT__cplusplus 202302L`, `cxxversionflag="-std=c++23"`, and the `cxx23`
  feature, while `6.38.06` and `6.36.14` for the same platform, and `6.40.04`
  and `6.36.14` for Ubuntu 24.04, all report `201703L`. The standard therefore
  changed recently within the 6.40 series and cannot be inferred from a release
  number, which is why each candidate was read directly.
- That 26.04 package is the clean target this branch's checks were written for:
  its `202302L` matches both Cling and Ubuntu 26.04's GCC 15.2, so no ROOT
  header warning remains anywhere. It also provides `asimage`, `asimage_tiff`,
  `imt`, `opengl`, `runtime_cxxmodules`, `tpython`, and `xml`. Downloaded in
  full (342,924,179 bytes) and measured SHA256
  `cd44b56ca02e00205b00bec11e207affda0fbbcc63dfd1e5bbd17286444ed4b9`.
  No automatic download default was changed on the strength of this inspection.
- Its PyROOT is compiled for **Python 3.14** (`cpython-314` bytecode), unlike
  BioDynaMo's Python 3.9 ROOT and ParaView 5.9 packages. Adopting it on Ubuntu
  26.04 is consequently also a Python and ParaView decision, matching the plan's
  existing caution against bundling an unrelated Python ABI change.
- Local validation of this package is not possible directly: it targets Ubuntu
  26.04's glibc and GCC 15.2 runtime. Current GitHub runners do not provide
  26.04 either, so validation used a container (see below).

## 2026-10-20 — Ubuntu 26.04 container validation

Validated the official package in `ubuntu:26.04` (Ubuntu 26.04.1 LTS, GCC
15.2.0, system CMake 4.2.3, system Python 3.14). Re-downloading the archive
reproduced the recorded SHA256.

- **The three-way `__cplusplus` agreement holds.** `g++ -std=c++23` reports
  `202302`, the installed `RConfigure.h` defines `ROOT__cplusplus 202302L`, and
  Cling reports `202302`. `root-config` reports
  `-pthread -std=c++23 -m64 -fsized-deallocation` and the `cxx23` feature.
  This is the combination the migration targets, and it cannot be reproduced
  with the GCC 13 package.
- **Compiling against ROOT's headers is warning-free.** A translation unit
  including `RConfigure.h` and `TROOT.h`, compiled with `-Wall -std=c++23` and
  linked against ROOT, produced no diagnostics at all and reported ROOT 6.40.04
  at runtime. The same source warns against the GCC 13 package. This confirms
  the warning is a package/compiler pairing problem, not an inherent cost of
  C++23.
- **BioDynaMo's language and ROOT policy accepts the package**, including the
  new `__cplusplus` comparison: configuration reports
  `ROOT 6.40.04: C++23 at /opt/root` and
  `ROOT and compiler __cplusplus: 202302`, and succeeds.
- Noted two harness details for future scripting. ROOT exits with a nonzero
  status after `-q -e`, which aborts any `pipefail` script that reads its
  output; and the extracted package needs `libtbb12` present before the
  interpreter can start, otherwise `TROOT::InitInterpreter` fails fatally.
- `gcc-11`, which the Ubuntu 26.04 prerequisite list inherits from 24.04 for the
  prebuilt interpreter's headers, is still available on 26.04 (11.5.0). The
  official package does not need it.
- **The Python pin is a hard configure-time blocker, not a soft concern.**
  `CMakeLists.txt` calls `find_package(Python ${pythonvers} EXACT ...)` and
  selects `3.9.25` for Ubuntu 26.04, matching the reused Ubuntu 24.04 archives.
  Configuring against the official package with the system interpreter fails
  with "We did not find the Python 3.9.25 interpreter from PyEnv", because
  Ubuntu 26.04 ships Python 3.14 and the official ROOT's PyROOT is built for it.
  Adopting that package therefore requires one of: raising the pin to the
  package's Python, keeping pyenv 3.9.25 and accepting that ROOT's Python
  bindings are built against a different interpreter, or continuing to build a
  BioDynaMo ROOT against Python 3.9. This is a project decision and no pin was
  changed here.
- Added an `ubuntu-2604-root-package` job to the migration workflow that runs
  these checks in a `ubuntu:26.04` container, since no hosted runner image
  provides 26.04. It downloads the package, verifies the recorded SHA256, and
  asserts the three-way `__cplusplus` agreement, a warning-free compile against
  ROOT's headers, and acceptance by `verify_ROOT()`. Dry-running the job's own
  steps in that container passes; an initial run also showed the job needs
  `make` installed before CMake can configure.

## 2026-10-20 — Ubuntu 26.04 build with GCC 15.2

Built BioDynaMo in the same container against the official C++23 ROOT, using the
pyenv Python 3.9.25 that the repository's 26.04 setup requires, so the C++23 and
GCC 15 questions are answered independently of the Python decision above.
Configuration was Release, `-Dtest=ON -Ddict=ON -Dparaview=OFF -Dnuma=ON`.

- Python 3.9.25 builds from source on Ubuntu 26.04 under GCC 15.2, so the
  existing 26.04 prerequisite path works on the real platform.
- Configuration succeeds against ROOT 6.40.04 with the C++23 and `__cplusplus`
  checks active. `git describe` warns only because the source snapshot excluded
  `.git`.
- **The C++23 sources compile cleanly under GCC 15.2.** All **136 compilation
  database entries carry exactly one `-std=c++23`**, matching the GCC 13 build,
  and the whole build produced four warnings: three pre-existing
  `-Woverloaded-virtual=` and one `-Winterference-size`. No errors, and no ROOT
  header standard warning anywhere, including during dictionary generation.
  This is the first evidence that the migration compiles on the motivating
  platform's own compiler.
- **All 439 unit tests pass in 79 suites, with zero failures and the same four
  existing disabled tests as the GCC 13 Debug run**, in about 34 seconds with
  two OpenMP threads. `RootStandardTest.InterpreterAndPublicHeadersAgree`
  passes, so Cling and compiled code agree on this platform too, and the test
  log contains no ROOT standard-mismatch warning at all. Log kept at
  `scratchpad/out/unit-tests.log`.
- Container caveat worth recording for CI and for users: Docker's default
  seccomp profile blocks the NUMA syscalls, so `numa_available()` returns -1
  even though `/sys/devices/system/node/node0` exists. A `numa=ON` build then
  aborts early in the unit suite with
  "Call to numa_available failed with return code: 1". Running with
  `--security-opt seccomp=unconfined` makes `numa_available()` return 0. This is
  a container policy limitation, unrelated to C++23.

## 2026-10-20 — Standard derived from the ROOT package

Reviewing how ROOT handles this upstream changed the policy. ROOT's
`cmake/modules/CheckCompiler.cmake` supports C++17, 20 and 23 from one source
tree, defaults to the compiler's default standard with a C++17 floor, and
hard-errors on anything else. Its published binaries differ per platform: the
measurements above show 6.40.04 is C++17 on Ubuntu 24.04 and C++23 on Ubuntu
26.04. GCC 15.2's own default is still `201703L`, so that 26.04 C++23 build is
an explicit choice by ROOT's release process, not an automatic consequence —
candidate packages must keep being inspected individually.

Pinning C++23 in BioDynaMo therefore fought upstream's model and had two
concrete costs: `bdm_root_download()` hard-errored on all Linux and all
pre-Xcode-26 macOS, dropping those platforms, and macOS already mixes modes
because the Xcode 26 packages are C++23 while older buckets are C++17. The
actual requirement is only that BioDynaMo match the ROOT package it links
against, not that it be C++23.

Changes:

- `cmake/CompilerStandard.cmake` no longer fixes the standard. It validates the
  invariants that hold regardless of mode (CMake 3.24+, no explicit `-std=`
  flags) and exposes `bdm_apply_cxx_standard()`, a macro so the standard lands
  in the caller's directory scope. Supported modes are 17 and 23;
  `BDM_SUPPORTED_CXX_STANDARDS` is the single place to widen that. An installed
  consumer applies `BioDynaMo_CXX_STANDARD`, which the use-file already
  substituted from the build.
- `bdm_check_root_standard()` now compares only when a standard is already
  fixed, which is the consumer case and an explicit request; otherwise the main
  build adopts `ROOT_CXX_STANDARD` straight after `verify_ROOT()`.
- The `__cplusplus` comparison moved out of `verify_ROOT()` to just after the
  standard is applied, because the probe needs the selected standard's own flag
  spelling. Its provisional-value warning now fires only for C++23, since all
  supported compilers agree on `201703L`.
- `bdm_root_download()` resolves the registered per-platform package again
  instead of refusing everything that is not a C++23 archive.
- The ROOT version floor is 6.22 again, with 6.36 required only when the
  selected package reports C++23.
- `BDM_CXX_STANDARD_COMPILE_OPTION` replaces the hardcoded
  `CMAKE_CXX23_STANDARD_COMPILE_OPTION` in `bdm-config`, the OptimLib build, and
  dictionary/consumer flags. `target_compile_features` publishes
  `cxx_std_${BDM_CXX_STANDARD}`.

Validation:

- **33 configuration tests pass** with CMake 3.27.9, including new coverage for
  derivation from a C++17 and a C++23 package, rejection of unsupported modes
  when applied, consumer application of the use-file standard, the 6.22 floor,
  and the C++23-only 6.36 rule.
- Configuring against the local ROOT 6.36.06 C++23 package derives C++23, and
  against the existing ROOT 6.30.02 package derives C++17 with all three
  `__cplusplus` values agreeing on `201703L`.
- **A full C++17 Debug build against ROOT 6.30.02 passes all 439 unit tests**,
  with the same four disabled tests and no failures, and all 136 compilation
  database entries carry exactly one `-std=c++17`. This is the configuration
  current releases use, and it builds on this branch again.

Building that path, rather than only configuring it, exposed two defects that
the pinned policy could not reach:

- `bdm-config` validates the ROOT a consumer sources against the standard
  BioDynaMo recorded, but its accepted spellings were hardcoded to the C++23
  set. In a C++17 build it rejected `-std=c++17` while reporting "ROOT must use
  C++17", which failed dictionary generation. It now derives the accepted
  spellings from the recorded standard and keeps the draft aliases, so a ROOT
  built with `-std=c++2b` still satisfies a C++23 build. Two regressions cover
  it: acceptance and refusal in both directions, and the draft spelling.
- The interpreter test asserted C++23 outright, both at compile time and for
  Cling. The invariant that actually matters is that compiled code and the
  interpreter are in the same language generation, whichever ROOT selected, so
  it now buckets `__cplusplus` into 17/20/23/26 and compares the two. That
  still tolerates GCC 13's provisional `202100L` against Cling's `202302L`
  while catching a real mismatch, and reports both values on failure.

These were both in code paths no C++23-only configuration could exercise, which
is the practical argument for keeping both modes in CI.

## Still pending
- Validate the official Ubuntu 26.04 C++23 package on a 26.04 system, and
  decide the Python/ParaView consequence before changing download defaults.
- For Ubuntu 22.04/24.04, either build a C++23 package with GCC 14 or newer, so
  it reports `202302L` like the 26.04 package, or record an explicit support
  decision. GCC 14 is not installed locally; only GCC 11, GCC 13, and Clang 18
  are available, and GCC 13 can only produce the provisional `202100L`.
- Cross-platform package builds, artifact publication, benchmark comparison,
  CUDA/OpenCL device validation, and the release support matrix.
