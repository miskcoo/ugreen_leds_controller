"""Run as root in an isolated Linux test environment; no hardware writes."""
import fcntl
import os
from pathlib import Path
import subprocess
import sys
import tempfile

cli = str(Path(sys.argv[1]).resolve())
lock = Path("/run/ugreen-leds-cli.lock")
if lock.exists() or lock.is_symlink():
    raise SystemExit("Refusing to replace an existing controller lock")

def run(*args):
    return subprocess.run([cli, *args], text=True, capture_output=True, timeout=5)

try:
    assert run("--help").returncode == 0
    assert not lock.exists()
    with lock.open("w") as stream:
        fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        result = run("all", "-status")
        assert result.returncode == 1 and "controller busy" in result.stderr, result
        assert run("--help").returncode == 0
    lock.unlink()
    with tempfile.TemporaryDirectory() as temp:
        target = Path(temp) / "target"
        target.write_text("unchanged")
        lock.symlink_to(target)
        result = run("all", "-status")
        assert result.returncode == 1 and "cannot safely open" in result.stderr, result
        assert target.read_text() == "unchanged"
        lock.unlink()
    lock.mkdir()
    result = run("all", "-status")
    assert result.returncode == 1 and "cannot safely open" in result.stderr, result
    lock.rmdir()
    lock.touch(mode=0o600)
    os.chown(lock, 65534, 65534)
    result = run("all", "-status")
    assert result.returncode == 1 and "cannot safely open" in result.stderr, result
finally:
    if lock.is_dir():
        lock.rmdir()
    else:
        lock.unlink(missing_ok=True)
print("CLI help, lock contention, symlink, directory and ownership checks passed")
