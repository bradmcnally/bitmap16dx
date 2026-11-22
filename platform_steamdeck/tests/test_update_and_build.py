"""Exercise build/install failures without touching the real checkout or app."""
import os
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "update_and_build.sh"
LAUNCHER = SCRIPT.with_name("run_bitmap16dx.sh")


class UpdateAndBuild(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "platform_steamdeck").mkdir()
        shutil.copy2(SCRIPT, self.root / "platform_steamdeck/update_and_build.sh")
        if LAUNCHER.exists():
            shutil.copy2(LAUNCHER, self.root / "platform_steamdeck/run_bitmap16dx.sh")
        (self.root / "third_party/picojson").mkdir(parents=True)
        (self.root / "third_party/picojson/picojson.h").write_text("JSON license")
        (self.root / "LICENSE").write_text("App license")
        self.bin = self.root / "commands"
        self.bin.mkdir()
        self.install = self.root / "installed"
        self.install.mkdir()
        (self.install / "bitmap16dx_desktop").write_text("previous working app")
        self.environment = os.environ.copy()
        self.environment.pop("BITMAP16_LOSPEC_FEED_URL", None)
        self.environment.pop("CONTAINER_ID", None)
        self.environment.pop("BITMAP16_STEAM_DECK_CONTAINER", None)
        self.environment.update(PATH=str(self.bin) + os.pathsep + os.environ["PATH"],
                                BITMAP16_STEAM_DECK_INSTALL_DIR=str(self.install))
        self.command("uname", "print('Linux' if sys.argv[1] == '-s' else 'x86_64')")
        self.command("git", "if sys.argv[1] == 'rev-parse': print('abc1234')")
        self.command("ctest", "sys.exit(int(os.environ.get('CHECK_FAILURE', '0')))")
        self.command("cmake", """
with open('configure-log.txt', 'a') as log: log.write(repr(sys.argv[1:]) + '\\n')
if '--build' in sys.argv:
    from pathlib import Path
    import struct
    Path('build_steamdeck').mkdir(exist_ok=True)
    if os.environ.get('WRONG_ARCH'):
        data = b'not a Linux executable'
    else:
        data = b'\\x7fELF\\x02\\x01\\x01' + bytes(9) + struct.pack('<HHI', 2, 62, 1) + bytes(40)
    Path('build_steamdeck/bitmap16dx_desktop').write_bytes(data)
    Path('build_steamdeck/music').mkdir(exist_ok=True)
    Path('build_steamdeck/music/track.mp3').write_bytes(b'music data')
""")

    def command(self, name, body):
        command = self.bin / name
        command.write_text(f"#!{sys.executable}\nimport os, sys\n" + body + "\n")
        command.chmod(0o755)

    def run_build(self, **overrides):
        return subprocess.run(["bash", str(self.root / "platform_steamdeck/update_and_build.sh")],
                              env=dict(self.environment, **overrides), capture_output=True, text=True)

    def test_success_updates_the_existing_shortcut_target(self):
        result = self.run_build()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((self.install / "bitmap16dx_desktop.bin").read_bytes(),
                         (self.root / "build_steamdeck/bitmap16dx_desktop").read_bytes())
        self.assertTrue(os.access(self.install / "bitmap16dx_desktop", os.X_OK))
        self.assertEqual((self.install / "music/track.mp3").read_bytes(), b'music data')
        self.assertEqual((self.install / "licenses/bitmap16dx-LICENSE").read_text(), "App license")
        self.assertIn("abc1234", (self.install / "build-info.txt").read_text())
        self.assertIn("-UBITMAP16_LOSPEC_FEED_URL", (self.root / "configure-log.txt").read_text())

    def test_explicit_feed_override_is_preserved(self):
        result = self.run_build(BITMAP16_LOSPEC_FEED_URL="https://example.test/palettes.json")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("-DBITMAP16_LOSPEC_FEED_URL=https://example.test/palettes.json",
                      (self.root / "configure-log.txt").read_text())

    def test_steam_shortcut_enters_the_runtime_container(self):
        result = self.run_build()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.command("distrobox", """
import json
from pathlib import Path
Path(os.environ['RUNTIME_ARGS']).write_text(json.dumps(sys.argv[1:]))
sys.exit(23)
""")
        recorded = self.root / "runtime-args.json"
        result = subprocess.run([str(self.install / "bitmap16dx_desktop"), "--windowed"],
                                env=dict(self.environment, RUNTIME_ARGS=str(recorded)),
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 23, result.stdout + result.stderr)
        self.assertEqual(json.loads(recorded.read_text()),
                         ["enter", "--name", "dev", "--no-tty", "--",
                          str(self.install / "bitmap16dx_desktop.bin"), "--windowed"])

    def test_launch_inside_the_runtime_container_does_not_reenter_it(self):
        result = self.run_build()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        binary = self.install / "bitmap16dx_desktop.bin"
        binary.write_text('#!/usr/bin/env bash\nprintf "native:%s\\n" "$1"\n')
        binary.chmod(0o755)
        self.command("distrobox", "sys.exit(99)")
        result = subprocess.run([str(self.install / "bitmap16dx_desktop"), "--windowed"],
                                env=dict(self.environment, CONTAINER_ID="dev"),
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "native:--windowed\n")

    def test_failed_checks_keep_the_installed_app(self):
        result = self.run_build(CHECK_FAILURE="7")
        self.assertEqual(result.returncode, 7)
        self.assertEqual((self.install / "bitmap16dx_desktop").read_text(), "previous working app")

    def test_wrong_architecture_keeps_the_installed_app(self):
        result = self.run_build(WRONG_ARCH="1")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("refusing to install", result.stderr)
        self.assertEqual((self.install / "bitmap16dx_desktop").read_text(), "previous working app")


if __name__ == "__main__":
    unittest.main()
