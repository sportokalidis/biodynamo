---
title: "C++23 migration plan"
date: "2026-10-15"
path: "/docs/devguide/cxx23-migration/"
meta_title: "BioDynaMo C++23 migration"
meta_description: "Proposed build, dependency, and validation changes for C++23."
toc: true
sidebar: "devguide"
---

## Status and recommendation

This work starts from repository revision `ab578242` (`master`). The branch makes
BioDynaMo's C++ standard **a property of the selected ROOT package** rather than
a fixed value, and enforces that BioDynaMo, its public headers, and ROOT's
interpreter agree on both the standard and the reported `__cplusplus`.

That policy follows ROOT's own: ROOT supports C++17, 20 and 23 from one source
tree, defaults to the compiler's default standard with a C++17 floor, rejects
anything else, and publishes different standards per platform and release. A
downstream consumer that pins one standard has to re-create packages for every
platform before anything works; deriving it instead lets each platform move when
its ROOT package moves.

Consequences:

- Ubuntu 22.04/24.04 and older Xcode versions keep building in C++17 against
  today's published packages. No new artifacts are needed for them.
- Platforms whose ROOT package is C++23, such as Xcode 26, build in C++23.
- BioDynaMo's own sources must stay C++17-compatible while any supported
  platform is C++17, so C++23 language features remain out of scope.

The [implementation log](cxx23_migration_log.md) records completed changes,
validation results, and unresolved gates. Validation covers a C++23 Debug build
(439 unit tests), a C++23 Release build with ParaView (458 unit tests plus the
full demo and system suite, installed-consumer smoke test, and a fixed-seed
numerical comparison against the C++17 baseline), a C++17 build against the
existing ROOT 6.30.02 package, and an Ubuntu 26.04 container build under GCC
15.2 against CERN's published C++23 ROOT 6.40.04.

Remaining gates concern packages rather than the language policy. Ubuntu 26.04
can use CERN's published C++23 ROOT 6.40.04, but adopting it as a download
default is blocked by BioDynaMo's exact Python pin, since that package's PyROOT
targets Python 3.14. See the package inventory below.

ROOT's version alone does not determine its C++ standard. Inspect the actual
package: ROOT documents that compiled applications and its Cling interpreter
must use the same standard, exposed through `ROOT_CXX_STANDARD` or
`root-config --cflags`.
See [ROOT's CMake integration documentation](https://root.cern/manual/integrate_root_into_my_cmake_project/).

## Findings at the baseline revision

| Area | Observed state | Consequence |
| --- | --- | --- |
| Core language mode | `cmake/SetCompilerFlags.cmake` checks `-std=c++17`, sets standard 17, requires it, and disables extensions. | This is the main switch, also included by downstream builds. |
| Build system | Top-level, demos, template, and system tests generally require CMake 3.19.3. Several installers also pin it. | CMake must be upgraded before using its C++23 standard property. |
| Linux ROOT | `bdm_root_platform()` in `cmake/utils.cmake` selects `root_v6.30.02_cxx17_python3.9_...` for every Ubuntu release, and `bdm_third_party_os()` maps Ubuntu 26.04 onto the Ubuntu 24.04 package. | Ubuntu 26.04 currently receives ROOT 6.30.02 in C++17; the Linux default must change. |
| macOS ROOT | Xcode 26 buckets select ROOT 6.36.06 or 6.40.04; comments identify both as C++23 despite `cxx17` filenames. Earlier buckets select other builds. | Inventory actual package metadata; filenames and release numbers are insufficient. |
| ROOT verification | `verify_ROOT()` accepts feature flags for 17, 20, or 23. Its freshly downloaded path skips that `else`-branch standard check. | Validate every discovery path after package resolution. |
| ROOT discovery | Custom `cmake/FindROOT.cmake` reads flags/features but does not expose `ROOT_CXX_STANDARD`. | Add normalized standard metadata and check command failures. |
| Generated projects | The simulation template and four demos set 17; `solar_system` sets 14. | Update downstream defaults and prevent later overrides from introducing mismatches. |
| Dictionaries | Custom ROOT/reflex commands in `cmake/FindROOT.cmake`, called by `cmake/BioDynaMo.cmake`. | CMake's target standard does not automatically configure these subprocesses. |
| CUDA | Top-level build explicitly passes `-std=c++17` to NVCC. | Decide and validate the host/device boundary separately. |
| External builds | GoogleTest 1.17.0, Google Benchmark 1.5.5, and OptimLib `18ae71` have independent build steps. | Parent CMake settings do not necessarily propagate. |
| CI | Ubuntu 22.04/24.04/26.04 and macOS 14/15/26 are listed in build workflows. | Preserve coverage or explicitly document any support changes. |

Local inspection found CMake **3.19.3**, GCC **13.3.0**, and
`build/third_party/root/bin/root-config` reporting ROOT **6.30.02**,
`-std=c++17`, and the `cxx17` feature. This environment cannot validate the final
configuration without newer CMake and a C++23 ROOT installation. Existing
`build/` and `build-paraview/` directories are not C++23 validation results.

## Prebuilt ROOT package inventory

CERN publishes prebuilt Linux binaries at [root.cern/download](https://root.cern/download/).
Each candidate below was inspected by reading `ROOT__cplusplus` from the
archive's own `include/RConfigure.h` and the `features` list from its
`bin/root-config`, rather than inferring a standard from the release number:

| Official package | `ROOT__cplusplus` | Standard |
| --- | --- | --- |
| `root_v6.40.04.Linux-ubuntu26.04-x86_64-gcc15.2.tar.gz` | `202302L` | **C++23** |
| `root_v6.38.06.Linux-ubuntu26.04-x86_64-gcc15.2.tar.gz` | `201703L` | C++17 |
| `root_v6.36.14.Linux-ubuntu26.04-x86_64-gcc15.2.tar.gz` | `201703L` | C++17 |
| `root_v6.40.04.Linux-ubuntu24.04-x86_64-gcc13.3.tar.gz` | `201703L` | C++17 |
| `root_v6.36.14.Linux-ubuntu24.04-x86_64-gcc13.3.tar.gz` | `201703L` | C++17 |

Two conclusions follow. The C++23 switch is recent even within the 6.40 series,
so every candidate must be inspected individually. And a suitable **prebuilt
C++23 package already exists for Ubuntu 26.04**: ROOT 6.40.04 built with GCC
15.2, reporting the final `202302L` and offering `cxx23`, `asimage`,
`asimage_tiff`, `imt`, `opengl`, `runtime_cxxmodules`, `tpython`, and `xml`.
Its SHA256 is
`cd44b56ca02e00205b00bec11e207affda0fbbcc63dfd1e5bbd17286444ed4b9`.
This matches the ROOT release the Xcode 26.6 bucket already selects, so the
same release can serve the newest macOS and Linux platforms.

Adopting it has one consequence to decide separately: its PyROOT is built for
**Python 3.14**, whereas BioDynaMo's own ROOT and ParaView 5.9 packages are
built for Python 3.9. Using the official package on Ubuntu 26.04 therefore
either requires a matching ParaView build or accepts ROOT and ParaView using
different Python ABIs. Resolve that before changing the download default.

Ubuntu 22.04 and 24.04 have no prebuilt C++23 option, so they need either a
BioDynaMo-built C++23 package or an explicit support decision. Build such a
package with GCC 14 or newer, available in Ubuntu 24.04's universe repository,
so it reports `202302L` like the 26.04 package rather than GCC 13's provisional
`202100L`.

## Planned changes

### 1. Toolchain and CMake requirements

- Raise the CMake minimum consistently in `CMakeLists.txt`,
  `util/simulation-template/CMakeLists.txt`, `demo/*/CMakeLists.txt`, and
  `test/system/*/CMakeLists.txt`. Review the separately configured
  `paraview_plugin/CMakeLists.txt` as well.
- CMake 3.20 introduced the `23` standard value. Propose **CMake 3.24 or newer**
  as the migration baseline, subject to the selected ROOT/toolchain's higher
  requirements. Check that baseline in CI rather than relying only on newer
  runner installations. See [CMake's CXX_STANDARD documentation](https://cmake.org/cmake/help/latest/prop_tgt/CXX_STANDARD.html).
- Update active CMake pins in `util/installation/ubuntu-{22.04,24.04}/prerequisites.sh`,
  `.github/workflows/sonar-source.yml`, and the third-party prerequisite/build
  scripts. Audit older Ubuntu/CentOS scripts and document their status rather
  than implicitly reintroducing support for obsolete configurations.
- Start validation with GCC 14+ or Clang 17+ on Linux and AppleClang supplied by
  the supported Xcode packages. These are candidate
  toolchains, not a claim of complete C++23 feature support. Record exact compiler,
  standard-library, SDK, OpenMP, and ROOT versions for each passing combination.
- Require agreement on the **value** of `__cplusplus`, not only on the selected
  standard. ROOT bakes its build compiler's value into the public `RConfigure.h`
  and warns from that header whenever a consumer disagrees; the same macro also
  selects preprocessor branches in ROOT's headers. GCC 13 reports the provisional
  `202100L`, whereas GCC 14, Clang 17, and ROOT's Cling report `202302L`. A ROOT
  package is therefore specific to the value its build compiler reported. Build
  published packages with a compiler reporting `202302L` so that compiled code,
  ROOT's headers, and the interpreter all agree, and treat GCC 13 as a probe
  rather than a supported toolchain. This matters most on Ubuntu 26.04, whose
  GCC reports `202302L` and would otherwise mismatch a GCC 13 package.
- Replace the C++17-only flag probe with CMake's C++23 capability checks and a
  small compile check where necessary. Keep `CMAKE_CXX_STANDARD_REQUIRED ON`
  and `CMAKE_CXX_EXTENSIONS OFF`. Test required features explicitly when adding
  them; accepting a standard flag is not proof of complete language/library support.
- Establish the standard policy early enough for checks, targets, and ROOT
  validation to use it. Currently ROOT verification precedes the compiler-flags
  include in important paths; avoid reading an unset standard there.

### 2. ROOT packages and compatibility checks

- Inventory each supported OS/architecture/Xcode bucket using the actual
  `root-config` output, compiler identity, standard library, Python ABI, and
  component availability. Select and pin tested ROOT releases per platform;
  a universal "latest ROOT" dependency is not a reproducible specification.
- Update `util/build-third-party/build-root.sh`, whose three configuration paths
  still request C++14, and `build-all.sh`, which defaults to ROOT 6.26.10.
  Build the selected releases with C++23 and a compatible toolchain. Verify their
  Cling can parse the selected compiler/SDK headers. See
  [ROOT's build instructions](https://root.cern.ch/install/build_from_source/).
- Build each published archive with a compiler reporting `__cplusplus` as
  `202302L` and record that value in the build manifest. The packaging script
  refuses to label an archive otherwise, because a package carrying the
  provisional `202100L` is usable only from the compiler that produced it.
- Produce and test C++23 ROOT archives before changing download defaults. Use
  accurate `cxx23` naming for new artifacts and record their real SHA256 digests
  in `cmake/external/SHA256Digests.cmake`. Update `bdm_root_platform()` and
  `bdm_root_platform_apple()` together with `cmake/external/ROOT.cmake`.
  Publishing these artifacts requires access to the dependency hosting service.
- Expose the detected standard in `cmake/FindROOT.cmake`. Read authoritative
  metadata from the selected installation, normalize flags such as `c++23` or
  `c++2b` if encountered, and reject ambiguous or contradictory results. Clear
  stale ROOT feature variables when rediscovering a package.
- Validate the standard after **external, cached, and newly downloaded** ROOT
  resolution. Ensure `root-config`, headers, libraries, `rootcling`, and
  `genreflex` belong to the same installation. Propagate new metadata out of
  `verify_ROOT()` when a download occurs.
- On mismatch, report the ROOT prefix/version, both standards, and how to select
  a compatible package. Never delete a user's external ROOT installation.
  Test cache replacement and a second configure to prevent repeated downloads.
- Review the current minimum ROOT version of 6.22 against the chosen supported
  releases. A version floor and a standard check serve different purposes.
- Retain macOS library-path repairs and component diagnostics. Changing the
  C++ standard does not itself repair unavailable image-export or TPython libraries.

### 3. Libraries, dictionaries, and downstream simulations

- Set BioDynaMo targets to C++23 and publish the requirement to consumers. Use
  `target_compile_features(... PUBLIC cxx_std_23)` where targets are exposed,
  alongside explicit standard enforcement in the existing package/use-file
  interface. A compile feature specifies a minimum; it does not enforce exact
  agreement with ROOT. See [CMake compile features](https://cmake.org/cmake/help/latest/manual/cmake-compile-features.7.html).
- Update `cmake/UseBioDynaMo.cmake.in`, `cmake/BioDynaMoConfig.cmake`, and
  `cmake/BioDynaMo.cmake` as needed to record the built standard and reject
  conflicting consumer settings. The current package primarily exposes a use
  file and plain library linkage; a complete imported-target redesign is optional.
- Update explicit settings in the template and the `analytic_continuum`, `mars`,
  `monolayer_growth`, `newtons_law_test`, and `solar_system` demos. Verify every
  other demo and system test inherits the same policy.
- Audit `ROOT_GENERATE_DICTIONARY` and `REFLEX_GENERATE_DICTIONARY`, generated
  code, `cmake/bdm-dictionary`, and `util/makefile-build/bdm-code-generation`.
  Confirm the selected ROOT tools use C++23; add supported explicit flags only
  where needed. Do not assume a custom command inherits target properties.
- Check runtime interpretation in `src/core/util/jit.{h,cc}`, rootlogon setup,
  and notebook paths. Compare compiled and interpreted language modes, parse
  BioDynaMo headers, instantiate a user agent, and execute generated JIT code.
- `cmake/env/bdm-config.in` derives its standard flag from the configured build.
  Verify it emits C++23 with no conflicting standard flag and that
  `demo/makefile_project/Makefile` builds against it.
- Regenerate dictionaries/PCM files and rebuild libraries and user simulations
  in fresh directories. Do not reuse artifacts from the C++17 configuration.

### 4. Source and third-party compatibility

- Compile core, neuroscience, demos, and tests in C++23; fix observed errors with
  small changes. Audit missing direct includes, overload resolution/comparisons,
  allocator APIs, deprecated facilities, and changed standard-library behavior.
  These are investigation areas, not established failures in BioDynaMo.
- Review `third_party/cxxopts-v2.2.1/cxxopts.h`, `third_party/json.hpp`,
  `third_party/cpptoml/cpptoml.h`, `third_party/opencl/cl2.hpp`, and extracted
  OptimLib/Armadillo headers under both libstdc++ and libc++. The cxxopts Unicode
  iterator uses `std::iterator` under `CXXOPTS_USE_UNICODE`; this is an audit item,
  not a demonstrated failure of the default configuration.
- Pass the chosen compiler/standard deliberately into CMake external projects
  in `cmake/Testing.cmake` and `cmake/Benchmarking.cmake`. Inspect OptimLib's own
  configure script before setting its compiler/flags. Retain a different mode
  only where a tested dependency boundary permits it.
- Upgrade vendored dependencies only when justified by build/runtime failures
  or toolchain support. Record versions, patches, licenses, and regression
  coverage; do not bundle broad dependency refreshes into the standard switch.
- Preserve simulation algorithms, random behavior, serialized class layouts,
  and numeric tolerances unless a separately explained fix requires changes.

### 5. Optional components and deployment environments

- **ParaView/VTK:** validate public headers, visualization dictionaries, Catalyst,
  and the separately built plugin against the shipped Linux 5.9/macOS 5.10
  packages. Rebuild or upgrade packages if evidence requires it. Matching the
  ROOT standard does not establish compatibility with these other libraries.
- **CUDA:** inspect shared headers and the NVCC/host compiler support matrix for
  the selected toolkit. Prefer a tested boundary allowing isolated kernels to
  remain in their supported language mode; otherwise require a suitable toolkit
  or fail `cuda=ON` clearly. Do not blindly pass `-std=c++23` to NVCC. GPU support
  needs its own build and runtime acceptance gate before being declared supported.
- **OpenCL, SBML, Boost, allocators, and benchmarks:** compile enabled variants
  and run their relevant tests. OpenCL kernel language settings are distinct
  from the C++23 mode of the host bindings.
- Update package lists, Ubuntu environment scripts, `.devcontainer/Dockerfile`,
  installation Dockerfiles, `Singularity`, and related workflows. GCC 11 packages
  currently also supply headers expected by the prebuilt ROOT interpreter;
  remove them only after replacement ROOT packages no longer need them.
- Keep Python selection aligned with ROOT/ParaView packages. Do not assume the
  C++ migration also permits an unrelated Python ABI change.

### 6. CI, documentation, and release behavior

- Update build and system-test workflows for Ubuntu and macOS. Log compiler,
  CMake, ROOT version/standard, and selected package identity. Make cache keys
  distinguish changed toolchains/standards/artifacts.
- Include GCC/libstdc++ and Clang/libc++ coverage, minimum supported tools, and
  the Xcode 26 package path that motivated the migration. Validate clean installs
  and external ROOT selection as well as cached builds.
- Update clang-tidy selection so it parses the selected language mode; inspect
  the old LLVM repository setup in Ubuntu CI. Run applicable formatting/lint
  checks without hiding migration failures behind existing `|| true` commands.
- Update `doc/dev_guide/build.md`, `doc/user_guide/prerequisites.md`, installation
  instructions, and `doc/user_guide/changelog.md`. Replace the statement that
  the ROOT standard mismatch is harmless when the exact-match policy lands.
- Explain the compiler/CMake/ROOT requirements, rebuilding custom simulations
  and plugins, cache cleanup via fresh build directories, and any support changes.
  Verify old backup files explicitly before promising cross-release compatibility.

## Implementation sequence and completion gates

Each step should be a reviewable commit or PR. Complete the dependency gate
before making C++23 the production default.

| Step | Deliverable | Gate |
| --- | --- | --- |
| 1 | Package/toolchain inventory and a baseline C++17 test record | Exact current configurations and known failures recorded. |
| 2 | C++23 ROOT recipes/artifacts and toolchain installer changes | Tested C++23 ROOT available for each retained default platform, each reporting `__cplusplus` as `202302L`. |
| 3 | Standard metadata, exact-match checks, CMake baseline, and focused configuration tests | Matching ROOT accepted; old, unknown, conflicting, and unsupported configurations fail clearly. |
| 4 | C++23 target settings, dictionary/consumer changes, and necessary source fixes | Clean core build, runtime JIT, unit tests, new simulation, and Makefile consumer pass. |
| 5 | Optional integration validation and any required dependency patches | ParaView/system tests pass; CUDA and other options have tested support or explicit diagnostics. |
| 6 | CI matrix, user documentation, release notes, and default rollout | Required matrix green, downloadable artifacts verified, and migration instructions exercised. |

## Validation checklist

- [x] Configuration tests cover C++17/20/23/unknown ROOT metadata, failed
  `root-config`, unsupported compiler/CMake, external and downloaded packages,
  cached package replacement, repeat configuration without a download loop, and
  `__cplusplus` agreement between ROOT and the selected compiler.
- [x] Inspect `compile_commands.json` and verbose dictionary commands for the
  effective standard. Verify the compiled application and interpreter agree.
- [x] Build Debug and Release with dictionaries enabled and run the unit suite.
  Include backup/restore, reflection/JIT, JSON/TOML parameters, custom agents,
  diffusion, and neuroscience coverage. Test `dict=OFF` separately.
  (Debug 439 tests; Release with ParaView 458 tests; `dict=OFF` runs a
  feature-appropriate 61-test subset.)
- [x] Run full demo/system tests with ParaView enabled, including the generated
  simulation, Makefile project, `test/system/backup_restore`, multiple simulations,
  and visualization output. The existing system-test runner expects PVSM output;
  use targeted tests for the initial `paraview=OFF` build.
  (All 19 runnable demos and the system tests pass; 12 verified PVSM outputs.
  SBML skips itself without libroadrunner.)
- [x] Smoke-test installed/relocated consumers outside the source tree and
  exercise ROOT loading plus notebook/Python integration where supported.
- [x] Validate optional build variants, `real_t=float` and `double`, and OpenMP
  behavior. Record skipped configurations explicitly.
  (CUDA and OpenCL remain unvalidated; no toolkit or headers are installed.)
- [ ] Compare representative fixed-seed simulation results and benchmarks with
  the recorded baseline; explain numerical/performance differences.
  (Fixed-seed results match exactly against both the Debug and Release C++23
  libraries; benchmark comparison is still outstanding.)
- [x] Exercise a legacy backup fixture separately from same-build round trips
  (C++17/ROOT 6.30.02 to C++23/ROOT 6.36.06; broader schema coverage remains open).
- [x] Run repository-required format/lint checks on implementation changes.
  (`show-format`, `check-cpplint`, and clang-tidy 18 report only two
  pre-existing `experimental::Style` findings.)

After obtaining matching dependencies and implementing the changes, an initial
CPU validation run can use the following commands from the repository root:

```bash
# First source the chosen C++23 ROOT's bin/thisroot.sh.
root-config --version
root-config --cflags
cmake --version
cmake -S . -B build-cxx23 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -Dtest=ON -Ddict=ON -Dparaview=OFF -Dcuda=OFF -Dopencl=OFF
cmake --build build-cxx23 --parallel
source build-cxx23/bin/thisbdm.sh
cmake --build build-cxx23 --target run-unit-tests
```

Repeat in a separate Release build with ParaView enabled for `run-demos` and
the full system suite. These are future validation instructions, not commands
that have passed as part of preparing this document.

## Decisions still to resolve

Choose exact ROOT/toolchain artifacts for each retained platform, confirm the
minimum compiler versions through builds, and settle CUDA support using the
selected toolkit. The minimum compiler decision now has a concrete constraint:
a package and its consumers must report the same `__cplusplus`, which rules out
pairing GCC 13 with the packages Ubuntu 26.04's compiler needs. Confirm GCC 14+
through an actual build and ROOT rebuild before fixing the support matrix. Any platform removal needs an explicit support decision.
Artifact hosting access is needed to finish distribution. If these gates cannot
be met, keep the default switch unmerged; roll back package selection and build
policy together instead of shipping a mismatched combination.

At planning time, preparation covered the source/configuration audit, local
toolchain and ROOT metadata, and upstream ROOT/CMake requirements. Current
implementation and validation results are recorded in the
[implementation log](cxx23_migration_log.md).
