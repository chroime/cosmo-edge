"""Execute the x86 workflow's routing and required-result shell against Git fixtures."""
import itertools
import os
from pathlib import Path
import re
import subprocess
import tempfile
import textwrap
import unittest


WORKFLOW = (Path(__file__).resolve().parents[1]
            / ".github/workflows/ci-build-and-test.yml").read_text()


def job(name):
    """Read one job block; these fixtures deliberately execute the workflow itself."""
    return re.search(rf"^  {re.escape(name)}:\n(.*?)(?=^  [\w-]+:|\Z)",
                     WORKFLOW.split("\njobs:\n", 1)[1], re.MULTILINE | re.DOTALL)[1]


def step_script(job_name, step_id):
    for step in re.split(r"(?m)^      - ", job(job_name)):
        if f"        id: {step_id}\n" in step:
            return textwrap.dedent(step.split("        run: |\n", 1)[1])
    raise AssertionError(f"Missing workflow step {job_name}/{step_id}")


class X86WorkflowTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="x86 workflow ")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name) / "repo"
        self.root.mkdir()
        self.output = Path(temporary.name) / "github-output"
        self.git("init", "-q")
        self.git("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                 "-c", "commit.gpgsign=false", "-c", "core.hooksPath=/dev/null",
                 "commit", "--allow-empty", "-qm", "base")
        self.base = self.git("rev-parse", "HEAD")

    def git(self, *args):
        result = subprocess.run(["git", *args], cwd=self.root, text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout.strip()

    def write(self, path):
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text("fixture\n")

    def commit(self):
        self.git("add", "-A")
        self.git("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
                 "-c", "commit.gpgsign=false", "-c", "core.hooksPath=/dev/null",
                 "commit", "--allow-empty", "-qm", "change")
        return self.git("rev-parse", "HEAD")

    def scope(self, expected, **env):
        self.output.unlink(missing_ok=True)
        result = subprocess.run(
            ["bash", "-e", "-o", "pipefail", "-c", step_script("scope", "scope")],
            cwd=self.root, text=True, capture_output=True,
            env={**os.environ, "EVENT_NAME": "pull_request", "BASE_SHA": self.base,
                 "HEAD_SHA": self.git("rev-parse", "HEAD"),
                 "GITHUB_OUTPUT": str(self.output), **env},
        )
        if expected is None:
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertFalse(self.output.exists(), "Failed detection must not publish a scope")
        else:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(self.output.read_text(), f"required={str(expected).lower()}\n")

    def test_unrelated_changes_do_not_require_compilation(self):
        for path in ("README.md", "docs/development/ci.md", "scripts/runtime_supervisor.py"):
            self.write(path)
        self.commit()
        self.scope(False)

    def test_existing_build_inputs_require_compilation(self):
        for path in (
            "src/top.cc", "src/nested/with space\nand newline.h", "test/test_case.py",
            "cmake/config.cmake", "3rd/dependency.h", "prebuild/ffmpeg/x86_64/lib/library.so",
            "toolchains/x86.cmake", "CMakeLists.txt", "scripts/build_cpu_test.sh",
            "tools/test_watchdog_driver.py", ".github/workflows/ci-build-and-test.yml",
        ):
            with self.subTest(path=path):
                self.base = self.git("rev-parse", "HEAD")
                self.write(path)
                self.commit()
                self.scope(True)

    def test_deleted_build_input_requires_compilation(self):
        self.write("src/removed.cc")
        self.base = self.commit()
        self.git("rm", "src/removed.cc")
        self.commit()
        self.scope(True)

    def test_rename_out_of_build_scope_still_requires_compilation(self):
        self.write("src/moved.cc")
        self.base = self.commit()
        self.git("mv", "src/moved.cc", "README.md")
        self.commit()
        self.scope(True)

    def test_base_branch_changes_are_not_part_of_pr_diff(self):
        self.write("README.md")
        head = self.commit()
        self.git("checkout", "--detach", self.base)
        self.write("src/base-only.cc")
        advanced_base = self.commit()
        self.scope(False, BASE_SHA=advanced_base, HEAD_SHA=head)

    def test_manual_dispatch_always_requires_compilation(self):
        self.scope(True, EVENT_NAME="workflow_dispatch", BASE_SHA="", HEAD_SHA="")

    def test_invalid_or_missing_refs_and_unknown_events_fail(self):
        for env in ({"BASE_SHA": "missing-ref"}, {"HEAD_SHA": "missing-ref"},
                    {"BASE_SHA": ""}, {"HEAD_SHA": ""}, {"EVENT_NAME": "push"}):
            with self.subTest(env=env):
                self.scope(None, **env)

    def test_required_result_only_accepts_verified_success_or_intentional_skip(self):
        states = ("success", "failure", "cancelled", "skipped")
        for scope_result, required, build_result in itertools.product(
                states, ("true", "false", "", "invalid"), states):
            with self.subTest(scope=scope_result, required=required, build=build_result):
                result = subprocess.run(
                    ["bash", "-e", "-o", "pipefail", "-c",
                     step_script("build-and-test", "result")],
                    cwd=self.root, text=True, capture_output=True,
                    env={**os.environ, "SCOPE_RESULT": scope_result,
                         "BUILD_REQUIRED": required, "BUILD_RESULT": build_result},
                )
                allowed = scope_result == "success" and (
                    (required == "true" and build_result == "success")
                    or (required == "false" and build_result == "skipped"))
                self.assertEqual(result.returncode == 0, allowed, result.stdout + result.stderr)

    def test_workflow_always_reports_the_existing_required_check(self):
        triggers = WORKFLOW.split("\non:\n", 1)[1].split("\npermissions:\n", 1)[0]
        self.assertIn("  pull_request:\n    branches: [main]\n", triggers)
        self.assertIn("  workflow_dispatch:\n", triggers)
        self.assertNotRegex(triggers, r"(?m)^\s+paths(?:-ignore)?:")
        self.assertNotRegex(job("build-and-test"), r"(?m)^    name:")
        self.assertIn("    if: always()\n", job("build-and-test"))
        self.assertIn("    needs: [scope, x86-tests]\n", job("build-and-test"))
        for variable, expression in (
            ("SCOPE_RESULT", "needs.scope.result"),
            ("BUILD_REQUIRED", "needs.scope.outputs.required"),
            ("BUILD_RESULT", "needs.x86-tests.result"),
        ):
            self.assertIn(f"          {variable}: ${{{{ {expression} }}}}\n", job("build-and-test"))
        self.assertIn("    needs: scope\n", job("x86-tests"))
        self.assertIn("    if: needs.scope.outputs.required == 'true'\n", job("x86-tests"))
        self.assertIn("      required: ${{ steps.scope.outputs.required }}\n", job("scope"))
        self.assertIn("          fetch-depth: 0\n", job("scope"))
        self.assertIn("run: python3 -m unittest discover -s test -p 'test_x86_workflow.py'",
                      job("scope"))


if __name__ == "__main__":
    unittest.main()
