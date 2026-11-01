import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from native.service.package_provenance import MANIFEST_NAME, write_manifest


NATIVE_ROOT = Path(__file__).resolve().parents[2]
INPUT_PATHS = (
    "service/streamget_service.py",
    "service/protocol.py",
    "service/douyu_backend.py",
    "service/requirements.txt",
    "service/requirements-build.txt",
    "service/requirements-lock-win-py313.txt",
    "service/verify_dependency_lock.py",
    "service/package_provenance.py",
    "scripts/bootstrap-streamget-service.ps1",
    "scripts/build-streamget-service.ps1",
    "app/resources/hamster_agent_roster.json",
)


class PackageProvenanceIntegrationTests(unittest.TestCase):
    def test_packaging_and_publish_paths_require_a_provenance_manifest(self):
        build_script = (NATIVE_ROOT / "scripts" / "build-streamget-service.ps1").read_text(
            encoding="utf-8"
        )
        copy_script = (NATIVE_ROOT / "cmake" / "copy_streamget_service.cmake").read_text(
            encoding="utf-8"
        )
        installer_script = (
            NATIVE_ROOT / "scripts" / "build-windows-installer.ps1"
        ).read_text(encoding="utf-8")

        self.assertIn("package_provenance.py", build_script)
        self.assertLess(
            build_script.index("if ($pyInstallerExitCode"),
            build_script.index("& $pythonExe $provenanceScript"),
        )
        self.assertIn("verify_streamget_service_package.cmake", copy_script)
        self.assertIn("verify_streamget_service_package.cmake", installer_script)

    def test_installer_validates_source_before_claiming_a_fresh_output(self):
        installer_script = (
            NATIVE_ROOT / "scripts" / "build-windows-installer.ps1"
        ).read_text(encoding="utf-8")

        source_assignment = installer_script.index("$serviceSourceDir =")
        source_verification = installer_script.index(
            "& cmake", source_assignment
        )
        output_command = "New-Item -ItemType Directory -Path $installerRoot"
        self.assertIn(output_command, installer_script)
        output_creation = installer_script.index(output_command)

        self.assertLess(source_assignment, source_verification)
        self.assertLess(source_verification, output_creation)
        self.assertNotIn("Remove-Item $installerRoot", installer_script)

    def test_installer_supports_separate_nonexistent_isolated_output_roots(self):
        installer_script = (
            NATIVE_ROOT / "scripts" / "build-windows-installer.ps1"
        ).read_text(encoding="utf-8")

        self.assertIn("[string]$StageRoot = ''", installer_script)
        self.assertIn("[string]$InstallerOutputDir = ''", installer_script)
        self.assertIn("$customOutputRoots", installer_script)
        self.assertIn("Test-Path -LiteralPath $stageRoot", installer_script)
        self.assertIn("Test-Path -LiteralPath $installerRoot", installer_script)


class PackageProvenanceVerificationTests(unittest.TestCase):
    def setUp(self):
        cmake = shutil.which("cmake")
        if not cmake:
            self.skipTest("CMake is required to verify package provenance")
        self.cmake = cmake
        base_dir = Path(os.environ.get("DOUYU_STEP47_TEMP", tempfile.gettempdir()))
        base_dir.mkdir(parents=True, exist_ok=True)
        self.test_root = Path(tempfile.mkdtemp(prefix="provenance-test-", dir=base_dir))
        self.package_dir = self.test_root / "streamget_service"
        self.package_dir.mkdir()
        self.exe_path = self.package_dir / "streamget_service.exe"
        self.exe_path.write_bytes(b"isolated package fixture")

        manifest = {
            "schemaVersion": 1,
            "inputs": {
                relative_path: self._sha256(NATIVE_ROOT / relative_path)
                for relative_path in INPUT_PATHS
                if (NATIVE_ROOT / relative_path).is_file()
            },
            "files": {
                "streamget_service.exe": {
                    "bytes": self.exe_path.stat().st_size,
                    "sha256": self._sha256(self.exe_path),
                },
            },
        }
        self._write_manifest(manifest)

    @staticmethod
    def _sha256(path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()

    def _write_manifest(self, manifest):
        path = self.package_dir / "build-provenance.json"
        path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    def _verify(self, package_dir=None):
        package_dir = package_dir or self.package_dir
        return subprocess.run(
            [
                self.cmake,
                f"-DPACKAGE_DIR:PATH={package_dir}",
                f"-DNATIVE_ROOT:PATH={NATIVE_ROOT}",
                "-P",
                str(NATIVE_ROOT / "cmake" / "verify_streamget_service_package.cmake"),
            ],
            capture_output=True,
            text=True,
            check=False,
        )

    def test_verifies_matching_source_and_payload_hashes(self):
        result = self._verify()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_manifest_records_current_inputs_and_built_package_files(self):
        package_dir = self.test_root / "generated"
        package_dir.mkdir()
        exe_path = package_dir / "streamget_service.exe"
        exe_path.write_bytes(b"manifest generator fixture")

        manifest_path = write_manifest(NATIVE_ROOT, package_dir)
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

        self.assertEqual(manifest_path.name, MANIFEST_NAME)
        self.assertEqual(set(manifest["inputs"]), set(INPUT_PATHS))
        self.assertEqual(
            manifest["inputs"]["service/requirements-lock-win-py313.txt"],
            self._sha256(NATIVE_ROOT / "service/requirements-lock-win-py313.txt"),
        )
        self.assertEqual(
            manifest["files"]["streamget_service.exe"],
            {"bytes": exe_path.stat().st_size, "sha256": self._sha256(exe_path)},
        )
        self.assertEqual(manifest["build"]["pythonVersion"], "3.13.15")
        self.assertEqual(manifest["build"]["streamgetVersion"], "4.0.10")
        self.assertEqual(manifest["build"]["pyinstallerVersion"], "6.22.0")

    def test_copy_script_verifies_source_and_destination_trees(self):
        destination = self.test_root / "release"
        result = subprocess.run(
            [
                self.cmake,
                f"-DSERVICE_SOURCE:PATH={self.package_dir}",
                f"-DSERVICE_DEST:PATH={destination}",
                "-P",
                str(NATIVE_ROOT / "cmake" / "copy_streamget_service.cmake"),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(
            self._sha256(destination / "streamget_service" / "streamget_service.exe"),
            self._sha256(self.exe_path),
        )

    def test_rejects_manifest_from_different_source_revision(self):
        path = self.package_dir / MANIFEST_NAME
        manifest = json.loads(path.read_text(encoding="utf-8"))
        manifest["inputs"]["service/protocol.py"] = "0" * 64
        self._write_manifest(manifest)

        result = self._verify()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("source hash mismatch", result.stdout + result.stderr)

    def test_rejects_payload_changed_after_manifest_creation(self):
        self.exe_path.write_bytes(b"changed payload")

        result = self._verify()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("file hash mismatch", result.stdout + result.stderr)

    def test_rejects_existing_unattested_service_package(self):
        unattested_package = self.test_root / "unattested-service"
        unattested_package.mkdir()
        (unattested_package / "streamget_service.exe").write_bytes(
            b"unattested package fixture"
        )

        result = self._verify(unattested_package)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("provenance manifest is missing", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
