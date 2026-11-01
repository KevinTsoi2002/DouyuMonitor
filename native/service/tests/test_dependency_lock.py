import subprocess
import tempfile
import unittest
from pathlib import Path


NATIVE_ROOT = Path(__file__).resolve().parents[2]
LOCK = NATIVE_ROOT / "service" / "requirements-lock-win-py313.txt"
CHECK = NATIVE_ROOT / "service" / "verify_dependency_lock.py"
VENV_PYTHON = NATIVE_ROOT / ".venv" / "Scripts" / "python.exe"


class DependencyLockTests(unittest.TestCase):
    def test_lock_is_used_by_bootstrap_and_checked_before_packaging(self):
        self.assertTrue(LOCK.is_file(), "StreamGet transitive dependency lock is missing")
        bootstrap = (NATIVE_ROOT / "scripts" / "bootstrap-streamget-service.ps1").read_text()
        build = (NATIVE_ROOT / "scripts" / "build-streamget-service.ps1").read_text()
        self.assertIn("requirements-lock-win-py313.txt", bootstrap)
        self.assertIn("--constraint $lockPath", bootstrap)
        self.assertIn("--requirement $lockPath", bootstrap)
        self.assertIn("verify_dependency_lock.py", bootstrap)
        self.assertIn("verify_dependency_lock.py", build)
        self.assertIn("[string]$DistPath", build)
        self.assertIn("[string]$WorkPath", build)

    def test_installed_dependency_closure_matches_lock(self):
        self.assertTrue(CHECK.is_file(), "StreamGet dependency verifier is missing")
        if not VENV_PYTHON.is_file():
            self.skipTest("StreamGet packaging virtual environment is unavailable")
        completed = subprocess.run(
            [str(VENV_PYTHON), str(CHECK)], capture_output=True, text=True, timeout=10
        )
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)

    def test_missing_http2_transitive_pin_is_rejected(self):
        if not VENV_PYTHON.is_file():
            self.skipTest("StreamGet packaging virtual environment is unavailable")
        with tempfile.TemporaryDirectory() as directory:
            lock = Path(directory) / LOCK.name
            lock.write_text("\n".join(line for line in LOCK.read_text(encoding="utf-8").splitlines()
                                      if not line.startswith("h2==")) + "\n", encoding="utf-8")
            completed = subprocess.run(
                [str(VENV_PYTHON), str(CHECK), "--lock-path", str(lock)],
                capture_output=True, text=True, timeout=10,
            )
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("h2", completed.stderr)


if __name__ == "__main__":
    unittest.main()
