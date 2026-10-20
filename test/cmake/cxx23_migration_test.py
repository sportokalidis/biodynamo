#!/usr/bin/env python3
"""Configuration regressions; no ROOT installation or network is needed.

Run: python3 test/cmake/cxx23_migration_test.py --cmake /path/to/cmake
Fake ROOT packages exercise discovery/cache policy, not ROOT runtime behavior.
"""
import argparse
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tarfile
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
CMAKE = "cmake"


def compiler_cplusplus():
    """The __cplusplus value this machine's C++ compiler reports for C++23."""
    result = subprocess.run(
        [os.environ.get("CXX", "c++"), "-std=c++23", "-E", "-P", "-x", "c++", "-"],
        input="__cplusplus\n", text=True, stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL)
    return result.stdout.strip().rstrip("L")


class MigrationTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="bdm-cxx23-")
        self.addCleanup(self.tmp.cleanup)
        self.work = Path(self.tmp.name)
        self.counter = 0

    def configure(self, body, expected=None, args=(), directory=None,
                  languages="NONE", standard="23"):
        """standard=None leaves BDM_CXX_STANDARD unset, as the main build does
        before it derives the language mode from the selected ROOT package."""
        self.counter += 1
        source = self.work / ("source" + str(self.counter))
        source.mkdir()
        preamble = ('cmake_minimum_required(VERSION 3.24)\n'
                    f'project(MigrationTest LANGUAGES {languages})\n'
                    f'list(PREPEND CMAKE_MODULE_PATH "{REPO}/cmake")\n'
                    'include(utils)\ninclude(ROOTCompatibility)\n')
        if standard:
            preamble += f'set(BDM_CXX_STANDARD {standard})\n'
        (source / "CMakeLists.txt").write_text(preamble + body)
        build = directory or self.work / ("build" + str(self.counter))
        result = subprocess.run([CMAKE, "-S", str(source), "-B", str(build), *args],
                                text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT)
        if expected:
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn(" ".join(expected.split()), " ".join(result.stdout.split()))
        else:
            self.assertEqual(result.returncode, 0, result.stdout)
        return build

    def root(self, standard="23", name="ROOT package", fail=None, version="6.36.06",
             cplusplus=None):
        prefix = self.work / name
        for folder in ("bin", "include", "lib", "etc"):
            (prefix / folder).mkdir(parents=True)
        if cplusplus:
            (prefix / "include/RConfigure.h").write_text(
                f"#define ROOT__cplusplus {cplusplus}L\n")
        values = {"--prefix": str(prefix), "--version": version,
                  "--incdir": str(prefix / "include"),
                  "--libdir": str(prefix / "lib"), "--etcdir": str(prefix / "etc"),
                  "--cflags": f"-pthread -std=c++{standard} -I{prefix}/include",
                  "--ldflags": "", "--features": f"cxx{standard} builtin_cling"}
        # Resolve the package's new prefix after extraction as real root-config does.
        script = '#!/bin/sh\nprefix=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)\ncase "$1" in\n'
        for option, value in values.items():
            if option == fail:
                script += f'{option}) echo broken >&2; exit 7;;\n'
            else:
                value = ("'" + value.replace("'", "'\"'\"'") + "'").replace(str(prefix), "'\"$prefix\"'")
                script += f'{option}) printf "%s\\n" {value};;\n'
        script += '*) exit 2;;\nesac\n'
        config = prefix / "bin/root-config"
        config.write_text(script)
        config.chmod(0o755)
        for tool in ("rootcling", "genreflex"):
            p = prefix / "bin" / tool
            p.write_text("#!/bin/sh\nexit 0\n")
            p.chmod(0o755)
        for lib in ("Core", "Geom", "Gui", "GenVector"):
            for suffix in ("so", "dylib"):
                (prefix / "lib" / (f"lib{lib}.{suffix}")).touch()
        return prefix

    def discover(self, root, tail=""):
        return (f'set(ROOT_CONFIG_EXECUTABLE "{root}/bin/root-config")\n'
                'find_package(ROOT REQUIRED COMPONENTS Geom Gui GenVector)\n'
                'verify_ROOT()\n' + tail)

    def test_standard_metadata(self):
        for flag, feature, standard in (("c++17", "cxx17", 17),
                                        ("gnu++20", "cxx20", 20),
                                        ("c++23", "cxx23", 23),
                                        ("gnu++2b", "cxx23", 23),
                                        ("c++2a", "cxx20", 20),
                                        ("c++1z", "cxx17", 17),
                                        ("c++2c", "cxx26", 26)):
            with self.subTest(flag=flag):
                self.configure(f'bdm_root_standard_from_metadata("-pthread -std={flag}" "{feature}" actual)\n'
                               f'if(NOT actual EQUAL {standard})\nmessage(FATAL_ERROR "wrong standard")\nendif()\n')

    def test_missing_and_conflicting_metadata(self):
        for flags, features in (("", ""), ("-std=c++17", "cxx23"),
                                ("-std=c++17 -std=c++23", "cxx23")):
            with self.subTest(flags=flags):
                self.configure(f'bdm_root_standard_from_metadata("{flags}" "{features}" actual)\n',
                               "Cannot determine a single ROOT C++ standard")
        self.configure('bdm_root_standard_from_metadata("-std=c++future" "" actual)\n',
                       "Unknown ROOT C++ standard")

    def test_matching_external_root(self):
        self.configure(self.discover(self.root()))

    def test_root_version_floor_and_legacy_spelling(self):
        self.configure(self.discover(self.root(version="6.36/06", name="slash")))
        self.configure(self.discover(self.root(version="6.20.10", name="ancient")),
                       "ROOT version must be 6.22 or newer")
        # A C++17 package older than 6.36 is fine; only C++23 needs 6.36+.
        self.configure(self.discover(self.root("17", version="6.30.02", name="c17")),
                       standard="17")
        self.configure(self.discover(self.root(version="6.34.10", name="too-old-23")),
                       "C++23 interpreter support requires ROOT 6.36 or newer")

    def test_standard_is_derived_from_the_package(self):
        # With no standard pinned, any supported package is accepted and its
        # language mode is reported back to the caller.
        for standard in ("17", "23"):
            root = self.root(standard, "derive" + standard)
            self.configure(self.discover(root) +
                           f'if(NOT ROOT_CXX_STANDARD EQUAL {standard})\n'
                           'message(FATAL_ERROR "wrong: ${ROOT_CXX_STANDARD}")\nendif()\n',
                           standard=None)

    def test_unsupported_standard_is_rejected_when_applied(self):
        for standard in ("20", "26"):
            self.configure('include(CompilerStandard)\n'
                           f'bdm_apply_cxx_standard({standard})\n',
                           f"BioDynaMo does not support C++{standard}",
                           languages="CXX", standard=None)

    def test_external_mismatches_remain_untouched(self):
        for standard in ("17", "20", "26"):
            root = self.root(standard, "root" + standard)
            self.configure(self.discover(root), "this build requires C++23")
            self.assertTrue((root / "bin/root-config").exists())

    def test_failed_metadata_query(self):
        self.configure(self.discover(self.root(fail="--cflags")), "--cflags failed")

    def test_missing_component(self):
        root = self.root()
        for p in (root / "lib").glob("libGui.*"):
            p.unlink()
        self.configure(self.discover(root), "missing: Gui")

    def test_missing_dictionary_tool(self):
        root = self.root()
        (root / "bin/genreflex").unlink()
        self.configure(self.discover(root), "GENREFLEX_EXECUTABLE")

    def test_rediscovery_discards_old_libraries_and_features(self):
        old = self.root("17", "old")
        new = self.root("23", "new")
        self.configure(
            f'set(ROOT_CONFIG_EXECUTABLE "{old}/bin/root-config")\n'
            'find_package(ROOT REQUIRED COMPONENTS Geom Gui GenVector)\n' +
            self.discover(new) +
            f'if(ROOT_cxx17_FOUND OR NOT ROOT_cxx23_FOUND OR NOT ROOT_Core_LIBRARY STREQUAL "{new}/lib/libCore${{CMAKE_SHARED_LIBRARY_SUFFIX}}")\n'
            'message(FATAL_ERROR "stale ROOT discovery")\nendif()\n')

    def test_external_prefix_is_not_a_managed_cache(self):
        root = self.root(name="third_party/root-external")
        self.configure(f'set(CMAKE_THIRD_PARTY_DIR "{self.work}/third_party")\n' + self.discover(root))
        self.assertTrue((root / "bin/root-config").exists())

    def test_unavailable_package_does_not_delete_old_cache(self):
        # Availability is resolved before a managed cache is replaced, so a
        # package that cannot be resolved leaves the old installation in place.
        root = self.root("17", "third_party/root")
        self.configure('set(APPLE FALSE)\nset(DETECTED_OS_VERS ubuntu-24.04)\n'
                       f'set(CMAKE_THIRD_PARTY_DIR "{self.work}/third_party")\n' +
                       self.discover(root),
                       "No ROOT SHA256 digest registered for 'ubuntu-24.04-ROOT'")
        self.assertTrue((root / "bin/root-config").exists())

    def test_custom_package_requires_checksum(self):
        self.configure('set(BDM_ROOT_URL file:///no-such-package.tar.gz)\n'
                       'bdm_root_download(url sha)\n', 'Set both BDM_ROOT_URL')

    def downloaded(self, standard, expected=None, stale=False, corrupt=False,
                   mirror=False, languages="NONE", cplusplus=None):
        root = self.root(standard, cplusplus=cplusplus)
        archive = self.work / "root.tar.gz"
        with tarfile.open(archive, "w:gz") as tar:
            for entry in root.iterdir():
                tar.add(entry, arcname=entry.name)
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if corrupt:
            digest = "0" * 64
        body = (f'set(CMAKE_THIRD_PARTY_DIR "{self.work}/managed")\n'
                f'set(BDM_ROOT_URL "{archive.as_uri()}")\n'
                f'set(BDM_ROOT_SHA256 "{digest}")\n'
                'set(ROOT_FOUND FALSE)\nverify_ROOT()\n')
        if mirror:
            body = (f'set(ENV{{BDM_LOCAL_LFS}} "{self.work}")\n' +
                    body.replace(archive.as_uri(),
                                 "https://example.invalid/biodynamo-lfs/root.tar.gz"))
        if stale:
            old = self.root("17", "managed/root")
            (old / "tar-sha256").write_text("old-digest")
            body = body.replace('set(ROOT_FOUND FALSE)\n',
                                f'set(ROOT_CONFIG_EXECUTABLE "{old}/bin/root-config")\n'
                                'find_package(ROOT REQUIRED COMPONENTS Geom Gui GenVector)\n')
        if not expected:
            body += ('if(NOT ROOT_CXX_STANDARD EQUAL 23 OR NOT ROOT_USE_FILE)\n'
                     'message(FATAL_ERROR "download metadata not propagated")\nendif()\n'
                     'file(WRITE "${CMAKE_THIRD_PARTY_DIR}/root/keep" "sentinel")\n'
                     'verify_ROOT()\n'
                     'if(NOT EXISTS "${CMAKE_THIRD_PARTY_DIR}/root/keep")\n'
                     'message(FATAL_ERROR "cache unnecessarily replaced")\nendif()\n')
        self.configure(body, expected, languages=languages)
        self.assertTrue(archive.exists(), "A local input archive was removed")

    def test_downloaded_root_and_repeat_verification(self):
        self.downloaded("23")

    def test_downloaded_mismatch_is_rejected(self):
        self.downloaded("17", "this build requires C++23")

    def test_stale_managed_package_is_replaced(self):
        self.downloaded("23", stale=True)

    # The download path resolves ROOT inside verify_ROOT(), so the check must
    # query the new installation instead of reading a discovery variable.
    def test_downloaded_root_is_compared_with_the_compiler(self):
        self.downloaded("23", languages="CXX", cplusplus=compiler_cplusplus())

    def test_downloaded_root_with_a_foreign_cplusplus_is_rejected(self):
        self.downloaded("23", languages="CXX", cplusplus="201703",
                        expected="built by a compiler reporting __cplusplus=201703")

    def test_corrupt_archive_is_rejected(self):
        self.downloaded("23", "SHA256", corrupt=True)

    def test_offline_mirror_extracts_and_verifies(self):
        self.downloaded("23", mirror=True)

    def test_offline_mirror_bad_checksum_is_rejected(self):
        self.downloaded("23", "SHA256", mirror=True, corrupt=True)

    def bdm_config(self, built, name):
        """Render bdm-config as a build with the given standard would."""
        install = self.work / name / "bin"
        install.mkdir(parents=True)
        template = (REPO / "cmake/env/bdm-config.in").read_text()
        template = template.replace("@cxxstd@", built)
        template = template.replace("@BDM_CXX_STANDARD_COMPILE_OPTION@",
                                    f"-std=c++{built}")
        config = install / "bdm-config"
        config.write_text(re.sub(r"@[A-Za-z_0-9]+@", "", template))
        return config

    def run_bdm_config(self, config, root):
        env = dict(os.environ,
                   PATH=str(root / "bin") + os.pathsep + os.environ["PATH"])
        return subprocess.run(["bash", str(config), "--cxxflags"], env=env,
                              text=True, stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE)

    def test_bdm_config_flags_and_changed_root(self):
        # bdm-config must accept the standard its build recorded and refuse any
        # other, for whichever standard that is.
        for built, other in (("23", "17"), ("17", "23")):
            config = self.bdm_config(built, "bdm" + built)
            with self.subTest(built=built):
                result = self.run_bdm_config(config, self.root(built, "ok" + built))
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.count("-std="), 1, result.stdout)
                self.assertIn(f"-std=c++{built}", result.stdout)

                result = self.run_bdm_config(config, self.root(other, "no" + built))
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(f"ROOT must use C++{built}", result.stderr)

    def test_bdm_config_accepts_the_draft_spelling(self):
        # A ROOT built by a compiler spelling the same standard -std=c++2b.
        config = self.bdm_config("23", "bdm")
        result = self.run_bdm_config(config, self.root("2b", "draft"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("-std=c++23", result.stdout)

    def test_launcher_pins_root_and_stops_on_failed_environment(self):
        install = self.work / "installed BDM"
        (install / "bin").mkdir(parents=True)
        root = self.work / "selected ROOT"
        template = (REPO / "cmake/env/launcher.sh").read_text()
        launcher = self.work / "launcher.sh"
        launcher.write_text(template.replace("@ROOTSYS@", str(root))
                           .replace("@CMAKE_INSTALL_ROOT@", str(install)))
        env = dict(os.environ, ROOTSYS="/old-root", BDM_ROOT_DIR="/old-root",
                   BDM_CUSTOM_ROOT="false")
        for status in (0, 7):
            (install / "bin/thisbdm.sh").write_text(
                f'[[ "$ROOTSYS" == "{root}" && "$BDM_ROOT_DIR" == "$ROOTSYS" '
                '&& "$BDM_CUSTOM_ROOT" == true ]] || return 9\n'
                f'return {status}\n')
            result = subprocess.run(
                ["bash", str(launcher), sys.executable, "-c",
                 "import sys; print(sys.argv[1])", "argument with spaces"],
                env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if status == 0:
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), "argument with spaces")
            else:
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, "")

    def rconfigure(self, root, body):
        (root / "include/RConfigure.h").write_text(body)
        return f'bdm_root_cplusplus_from_header("{root}/include" actual)\n'

    def test_root_cplusplus_macro_is_read_from_the_header(self):
        root = self.root()
        self.configure(self.rconfigure(root, '#define ROOT__cplusplus 202302L\n')
                       + 'if(NOT actual EQUAL 202302)\n'
                       'message(FATAL_ERROR "wrong value: ${actual}")\nendif()\n')
        # An absent macro is reported as empty rather than as a parse failure.
        self.configure(self.rconfigure(root, '#define R__HAS_PTHREAD\n')
                       + 'if(NOT actual STREQUAL "")\n'
                       'message(FATAL_ERROR "unexpected value: ${actual}")\nendif()\n')
        self.configure(self.rconfigure(
            root, '#define ROOT__cplusplus 202302L\n#define ROOT__cplusplus 202100L\n'),
            "defines ROOT__cplusplus more than once")

    def test_missing_rconfigure_header_is_rejected(self):
        root = self.root()
        self.configure(f'bdm_root_cplusplus_from_header("{root}/include" actual)\n',
                       "RConfigure.h' is missing")

    def test_cplusplus_values_must_agree(self):
        # Equal provisional values are accepted, but the interpreter disagrees.
        self.configure('bdm_check_cplusplus_agreement(202100 202100)\n')
        self.configure('bdm_check_cplusplus_agreement(202302 202302)\n')
        self.configure('bdm_check_cplusplus_agreement(202100 202302)\n',
                       "reports 202302")
        self.configure('bdm_check_cplusplus_agreement(202302 202100)\n',
                       "reports 202100")

    def test_compiler_cplusplus_matches_a_direct_probe(self):
        expected = subprocess.run(
            [os.environ.get("CXX", "c++"), "-std=c++23", "-E", "-P", "-x", "c++", "-"],
            input="__cplusplus\n", text=True, stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL).stdout.strip().rstrip("L")
        self.configure('include(CompilerStandard)\n'
                       'bdm_apply_cxx_standard(23)\n'
                       'bdm_compiler_cplusplus(actual)\n'
                       f'if(NOT actual EQUAL {expected})\n'
                       'message(FATAL_ERROR "wrong value: ${actual}")\nendif()\n',
                       languages="CXX", standard=None)

    def test_compiler_policy_and_public_feature(self):
        build = self.configure(
            'include(CompilerStandard)\n'
            'bdm_apply_cxx_standard(23)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/main.cc" "static_assert(__cplusplus > 202002L); int main() {}")\n'
            'add_executable(probe "${CMAKE_BINARY_DIR}/main.cc")\n',
            languages="CXX", standard=None)
        result = subprocess.run([CMAKE, "--build", str(build)], text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_conflicting_consumer_standard(self):
        # The installed use-file sets BioDynaMo_CXX_STANDARD to the standard
        # BioDynaMo was built with; a consumer pinning another one must fail.
        self.configure('set(CMAKE_CXX_STANDARD 17)\n'
                       'set(BioDynaMo_CXX_STANDARD 23)\n'
                       'include(CompilerStandard)\n',
                       "CMAKE_CXX_STANDARD=17", languages="CXX", standard=None)

    def test_consumer_standard_is_applied_from_the_use_file(self):
        for standard in ("17", "23"):
            self.configure(f'set(BioDynaMo_CXX_STANDARD {standard})\n'
                           'include(CompilerStandard)\n'
                           f'if(NOT CMAKE_CXX_STANDARD EQUAL {standard})\n'
                           'message(FATAL_ERROR "wrong: ${CMAKE_CXX_STANDARD}")\nendif()\n',
                           languages="CXX", standard=None)

    def test_conflicting_flag(self):
        self.configure('set(CMAKE_CXX_FLAGS "-std=c++17")\ninclude(CompilerStandard)\n',
                       "Remove the explicit standard flag", languages="CXX")

    def test_unsupported_compiler(self):
        self.configure('set(CMAKE_CXX_COMPILE_FEATURES cxx_std_17)\n'
                       'include(CompilerStandard)\nbdm_apply_cxx_standard(23)\n',
                       "does not support the C++23 mode", languages="CXX",
                       standard=None)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmake", default="cmake")
    args, remaining = parser.parse_known_args()
    CMAKE = args.cmake
    unittest.main(argv=[__file__, *remaining])
