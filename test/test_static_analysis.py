"""Exercise the static-analysis CLI, hook and CI against isolated Git fixtures."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest

REPOSITORY = Path(__file__).resolve().parents[1]
REAL_CPPCHECK = shutil.which("cppcheck")
BASH = shutil.which("bash")


class StaticAnalysisTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="cppcheck repo ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        for directory in ("scripts", "src", "test", "bin", "tmp"):
            (self.root / directory).mkdir()
        for name in ("static_analysis.sh", "pre-commit"):
            shutil.copy2(REPOSITORY / "scripts" / name, self.root / "scripts" / name)
        shutil.copy2(REPOSITORY / ".cppcheck-suppressions", self.root)
        self.write_executable("scripts/format_check.sh", "#!/bin/bash\nexit ${FORMAT_EXIT:-0}\n")
        # Use a closed PATH so missing-tool checks do not depend on the host.
        for name in ("bash", "dirname", "git", "find", "sort", "nproc", "mktemp",
                     "rm", "readlink", "grep", "head"):
            (self.root / "bin" / name).symlink_to(shutil.which(name))
        self.log = self.root / "calls.jsonl"
        self.env = dict(os.environ, PATH=str(self.root / "bin"),
                        TMPDIR=str(self.root / "tmp"), CALL_LOG=str(self.log))
        self.install_tool("cppcheck")
        self.git("init", "-q")
        self.commit()
        (self.root / ".git/hooks/pre-commit").symlink_to("../../scripts/pre-commit")

    def write(self, relative, content="int answer() { return 42; }\n"):
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
        return path

    def write_executable(self, relative, content):
        path = self.write(relative, content)
        path.chmod(0o755)

    def install_tool(self, name):
        self.write_executable("bin/" + name, f"#!{sys.executable}\n" + textwrap.dedent("""\
            import json, os, sys
            from pathlib import Path
            tool = Path(sys.argv[0]).name
            args = sys.argv[1:]
            with open(os.environ['CALL_LOG'], 'a') as log:
                log.write(json.dumps([tool, *args]) + '\\n')
            if args == ['--version']:
                print(tool + ' fixture version')
                sys.exit(int(os.environ.get('VERSION_EXIT', '0')))
            key = 'CPPCHECK_EXIT' if tool == 'cppcheck' else 'CLANG_EXIT'
            sys.exit(int(os.environ.get(key, '0')))
            """))

    def git(self, *args):
        result = subprocess.run([str(self.root / "bin/git"), *args], cwd=self.root,
                                env=self.env, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout.strip()

    def commit(self):
        self.git("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                 "-c", "commit.gpgsign=false", "-c", "core.hooksPath=/dev/null",
                 "commit", "--allow-empty", "-qm", "fixture")

    def run_command(self, command, expected=0, **env):
        self.log.unlink(missing_ok=True)
        result = subprocess.run(command, cwd=self.root, env={**self.env, **env},
                                text=True, capture_output=True)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        self.assertEqual(list((self.root / "tmp").iterdir()), [], "file-list leak")
        return result.stdout + result.stderr

    def analyze(self, *args, expected=0, **env):
        return self.run_command([BASH, str(self.root / "scripts/static_analysis.sh"), *args],
                                expected=expected, **env)

    def calls(self, tool="cppcheck"):
        records = [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []
        return [record[1:] for record in records if record[0] == tool and record[1:] != ["--version"]]

    def assert_files(self, expected):
        calls = self.calls()
        self.assertEqual(len(calls), 1, calls)
        files = {arg for arg in calls[0] if arg.endswith((".h", ".cc"))}
        self.assertEqual(files, {str(self.root / name) for name in expected})
        return calls[0]

    def test_profiles_and_full_scope(self):
        included = ("src/top.cc", "src/nested/with space.h", "test/check.cc")
        excluded = ("other.cc", "src/3rd/vendor.cc", "src/build/generated.h",
                    "test/catch_amalgamated.cc", "src/ignore.cpp")
        for name in (*included, *excluded):
            self.write(name)
        for profile, enabled in ((None, "warning,style,performance,portability"),
                                 ("full", "warning,style,performance,portability"),
                                 ("commit", "warning")):
            with self.subTest(profile=profile):
                self.analyze("--cppcheck", *(["--profile", profile] if profile else []))
                args = self.assert_files(included)
                self.assertIn("--enable=" + enabled, args)
                self.assertIn("--error-exitcode=1", args)
                self.assertIn("--suppressions-list=" + str(self.root / ".cppcheck-suppressions"), args)

    def test_staged_scope_spaces_and_deleted_files(self):
        self.write("src/deleted.cc")
        self.git("add", "src/deleted.cc")
        self.commit()
        self.git("rm", "src/deleted.cc")
        included = ("src/space name.cc", 'test/quoted "name".h', "other/staged.cc")
        for name in (*included, "src/ignored.cpp", "src/unstaged.cc"):
            self.write(name)
        self.git("add", "--", *included, "src/ignored.cpp")
        self.analyze("--cppcheck", "--staged")
        self.assert_files(included)

    def test_base_ref_scope_renames_and_deleted_files(self):
        for name in ("src/deleted.cc", "src/old.cc", "src/unchanged.h"):
            self.write(name)
        self.git("add", "src")
        self.commit()
        base = self.git("rev-parse", "HEAD")
        self.git("rm", "src/deleted.cc")
        self.git("mv", "src/old.cc", "src/renamed file.cc")
        included = ("src/top.h", "src/nested/change.cc", "test/top.cc", "test/nested/test.h")
        for name in (*included, "other/ignored.cc", "src/ignored.cpp"):
            self.write(name)
        self.git("add", "src", "test", "other")
        self.commit()
        self.write("src/staged-only.cc")
        self.git("add", "src/staged-only.cc")
        self.analyze("--cppcheck", "--base-ref", base)
        self.assert_files((*included, "src/renamed file.cc"))

    def test_empty_and_deleted_only_sets_succeed_without_analysis(self):
        for args in ((), ("--staged",), ("--base-ref", "HEAD")):
            self.assertIn("No files to analyze", self.analyze("--cppcheck", *args))
            self.assertEqual(self.calls(), [])
        self.write("src/deleted.cc")
        self.git("add", "src/deleted.cc")
        self.commit()
        base = self.git("rev-parse", "HEAD")
        self.git("rm", "src/deleted.cc")
        self.analyze("--cppcheck", "--staged")
        self.assertEqual(self.calls(), [])
        self.commit()
        self.analyze("--cppcheck", "--base-ref", base)
        self.assertEqual(self.calls(), [])

    def test_invalid_arguments_and_refs_fail(self):
        for args in (("--profile",), ("--profile", "unknown"), ("--base-ref",),
                     ("--base-ref", ""), ("--base-ref", "--staged"),
                     ("--staged", "--base-ref", "HEAD"),
                     ("--base-ref", "HEAD", "--staged")):
            with self.subTest(args=args):
                self.analyze("--cppcheck", *args, expected=1)
                self.assertEqual(self.calls(), [])
        for mode, status in (("--cppcheck", 128), ("--all", 1)):
            self.analyze(mode, "--base-ref", "no-such-ref", expected=status)
            self.assertEqual(self.calls(), [])

    def test_collection_errors_fail_instead_of_empty_success(self):
        (self.root / ".git").rename(self.root / "git-backup")
        self.analyze("--cppcheck", "--staged", expected=129)
        self.assertEqual(self.calls(), [])
        (self.root / "src").rmdir()
        self.analyze("--all", expected=1)
        self.assertEqual(self.calls(), [])

    def test_tool_failures_propagate_without_success_message(self):
        self.write("src/clean.cc")
        for status in (1, 17):
            output = self.analyze("--cppcheck", expected=status, CPPCHECK_EXIT=str(status))
            self.assertNotIn("No issues found", output)
        output = self.analyze("--all", expected=1, VERSION_EXIT="23")
        self.assertNotIn("No issues found", output)
        self.assertEqual(self.calls(), [])

    def test_missing_tools_direct_failure_and_optional_skips(self):
        (self.root / "bin/cppcheck").unlink()
        self.write("src/staged.cc")
        self.git("add", "src/staged.cc")
        self.analyze("--cppcheck", "--staged", expected=1)
        self.run_command([str(self.root / ".git/hooks/pre-commit")])
        output = self.analyze("--all")
        self.assertIn("Skipping cppcheck", output)
        self.assertIn("Skipping clang-tidy", output)
        self.assertEqual(self.calls(), [])

    def test_all_aggregates_failures_and_continues(self):
        self.write("src/clean.cc")
        self.write("build/compile_commands.json", "[]")
        self.install_tool("clang-tidy")
        self.install_tool("run-clang-tidy")
        for cpp, clang in ((0, 0), (17, 0), (0, 19), (17, 19)):
            with self.subTest(cpp=cpp, clang=clang):
                output = self.analyze("--all", expected=int(bool(cpp or clang)),
                                      CPPCHECK_EXIT=str(cpp), CLANG_EXIT=str(clang))
                self.assertEqual(len(self.calls()), 1)
                self.assertEqual(len(self.calls("run-clang-tidy")), 1)
                self.assertIn("Compiler Warning Summary", output)

    def test_hook_uses_commit_profile_and_blocks_failure(self):
        self.write("src/staged file.cc")
        self.git("add", "src/staged file.cc")
        for status in (0, 1, 17):
            self.run_command([str(self.root / ".git/hooks/pre-commit")],
                             expected=int(bool(status)), CPPCHECK_EXIT=str(status))
            args = self.assert_files(("src/staged file.cc",))
            self.assertIn("--enable=warning", args)
        self.run_command([str(self.root / ".git/hooks/pre-commit")], expected=1, FORMAT_EXIT="1")
        self.assertEqual(self.calls(), [])

    def test_ci_block_uses_merge_base_full_profile_and_propagates_failure(self):
        workflow = (REPOSITORY / ".github/workflows/pr-checks.yml").read_text()
        block = workflow.split("      - name: Run cppcheck (changed files only)\n", 1)[1]
        command = textwrap.dedent(block.split("        run: |\n", 1)[1].split("\n      - name:", 1)[0])
        self.git("update-ref", "refs/remotes/origin/main", "HEAD")
        self.write("test/changed file.cc")
        self.git("add", "test/changed file.cc")
        self.commit()
        self.run_command([BASH, "-e", "-o", "pipefail", "-c", command],
                         expected=17, CPPCHECK_EXIT="17")
        args = self.assert_files(("test/changed file.cc",))
        self.assertIn("--enable=warning,style,performance,portability", args)

    @unittest.skipUnless(REAL_CPPCHECK, "cppcheck is not installed")
    def test_real_diagnostics_profiles_and_suppressions(self):
        (self.root / "bin/cppcheck").unlink()
        (self.root / "bin/cppcheck").symlink_to(REAL_CPPCHECK)
        source = self.write("src/diagnostic file.cc")
        self.git("add", "src/diagnostic file.cc")
        cases = (
            ("error", "int f() { return 1 / 0; }\n", "zerodiv"),
            ("warning", "int f(const int *p) { int x = *p; if (p) return x; return 0; }\n", "nullPointerRedundantCheck"),
            ("style", "int f() { int unused = 1; return 0; }\n", "unreadVariable"),
        )
        for severity, code, diagnostic in cases:
            source.write_text(code)
            for profile in ("commit", "full"):
                with self.subTest(severity=severity, profile=profile):
                    status = int(severity != "style" or profile == "full")
                    output = self.analyze("--cppcheck", "--staged", "--profile", profile, expected=status)
                    if status:
                        self.assertIn("[" + severity + "]", output)
                        self.assertIn(diagnostic, output)
            (self.root / ".cppcheck-suppressions").write_text(diagnostic + "\n")
            self.analyze("--cppcheck", "--staged")
            (self.root / ".cppcheck-suppressions").unlink()
        source.write_text("int f() {\n// cppcheck-suppress zerodiv\nreturn 1 / 0;\n}\n")
        self.analyze("--cppcheck", "--staged")
        (self.root / ".cppcheck-suppressions").write_text("invalid[suppression\n")
        self.analyze("--cppcheck", "--staged", expected=1)


if __name__ == "__main__":
    unittest.main()
