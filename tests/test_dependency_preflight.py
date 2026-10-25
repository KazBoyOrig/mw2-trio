"""Own files and injected Windows errors only. No DLL loading or game launch."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import play_pair as tool


class DependencyPreflightTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="own-dependency-fixture-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.source.mkdir()
        (self.source / "main").mkdir()
        (self.source / "zone").mkdir()
        (self.source / "iw4sp.exe").write_bytes(b"OWN NONEXECUTABLE EXE FIXTURE")
        self.dll = self.source / "steam_api.dll"
        self.dll.write_bytes(b"OWN NONEXECUTABLE DEPENDENCY FIXTURE")
        self.original = tool.digest
        self.patch = mock.patch.object(tool, "EXE_SHA", self.original(self.source / "iw4sp.exe"))
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def blocked(self, code):
        error = OSError("own injected Windows failure")
        error.winerror = code
        def digest(path):
            if Path(path).name == "steam_api.dll":
                raise error
            return self.original(path)
        return mock.patch.object(tool, "digest", side_effect=digest)

    def test_readable_dependency_is_read_only_and_does_not_certify_safety(self):
        before = self.dll.read_bytes()
        self.assertEqual(tool.steam_dependency(self.source), hashlib.sha256(before).hexdigest())
        self.assertEqual(self.dll.read_bytes(), before)
        self.assertEqual(sorted(p.name for p in self.root.iterdir()), ["source"])

    def test_missing_dependency_stops_before_destination_creation(self):
        self.dll.unlink()
        target = self.root / "copy"
        with self.assertRaisesRegex(tool.Refused, "steam_api.dll is missing"):
            tool.prepare(str(self.source), str(target), True)
        self.assertFalse(target.exists())

    def test_windows_infected_error_stops_before_large_copy(self):
        target = self.root / "copy"
        with self.blocked(225), self.assertRaisesRegex(tool.Refused, "security blocked.*225"):
            tool.prepare(str(self.source), str(target), True)
        self.assertFalse(target.exists())

    def test_windows_deleted_error_is_explicit(self):
        with self.blocked(226), self.assertRaisesRegex(tool.Refused, "security blocked.*226"):
            tool.steam_dependency(self.source)

    def test_access_denied_does_not_claim_antivirus_detection(self):
        with self.blocked(5), self.assertRaisesRegex(tool.Refused, "does not establish an antivirus"):
            tool.steam_dependency(self.source)

    def test_unknown_io_failure_is_not_treated_as_readable(self):
        with self.blocked(32), self.assertRaisesRegex(tool.Refused, "Cannot read required"):
            tool.steam_dependency(self.source)

    def test_same_size_dependency_change_refuses_before_game_or_staging(self):
        target = self.root / "copy"
        tool.prepare(str(self.source), str(target), True)
        dll = target / "steam_api.dll"
        data = dll.read_bytes()
        dll.write_bytes(b"X" + data[1:])
        with self.assertRaisesRegex(tool.Refused, "differs from the prepared-copy inventory"):
            tool.verify_copy(target)
        self.assertFalse((target / tool.ACTIVE).exists())

    def test_unchanged_copy_keeps_existing_provenance_compatible(self):
        target = self.root / "copy"
        tool.prepare(str(self.source), str(target), True)
        tool.verify_copy(target)
        marker = json.loads((target / tool.MARKER).read_text())
        self.assertEqual(marker["files"]["steam_api.dll"]["sha256"], self.original(self.dll))

    def test_malformed_dependency_hash_refuses(self):
        for expected in ("not-a-hash", "", "A" * 64, "0" * 64):
            with self.subTest(expected=expected), self.assertRaises(tool.Refused):
                tool.steam_dependency(self.source, expected)

    def test_unknown_expected_hash_does_not_become_a_trust_verdict(self):
        self.assertEqual(tool.steam_dependency(self.source, self.original(self.dll)), self.original(self.dll))

    def test_dependency_change_after_preflight_leaves_no_launch_marker(self):
        target = self.root / "copy"
        original_copy = tool.copy_new
        def copy(source, destination):
            if Path(source).name == "steam_api.dll":
                data = self.dll.read_bytes()
                self.dll.write_bytes(b"Y" + data[1:])
            return original_copy(source, destination)
        with mock.patch.object(tool, "copy_new", side_effect=copy), self.assertRaisesRegex(tool.Refused, "changed after preflight"):
            tool.prepare(str(self.source), str(target), True)
        self.assertTrue(target.exists())
        self.assertFalse((target / tool.MARKER).exists())

    def test_missing_hash_inventory_refuses(self):
        target = self.root / "copy"
        tool.prepare(str(self.source), str(target), True)
        path = target / tool.MARKER
        marker = json.loads(path.read_text())
        del marker["files"]["steam_api.dll"]["sha256"]
        path.write_text(json.dumps(marker))
        with self.assertRaisesRegex(tool.Refused, "Incomplete dependency inventory"):
            tool.verify_copy(target)


if __name__ == "__main__":
    unittest.main()
