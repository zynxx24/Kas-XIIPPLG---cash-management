#!/usr/bin/env python3
"""
Auto push for the class cash data.

Every time data/*.json changes (an admin edits payments, expenses, donations...),
this script:
  1. commits and pushes data/ to this repo (Kas-XIIPPLG---cash-management),
  2. rebuilds the static site with export_static.py,
  3. commits and pushes dist/ to the export repo (XII-PPLG---CASH-EXPORTED),
     which GitHub Pages serves.

So the latest data is always on GitHub: on another laptop, clone this repo and
run ./run.sh local, and you continue from the same data.

    python3 autopush.py           # sync once now
    python3 autopush.py --watch   # keep watching data/ (run.sh starts this)
    python3 autopush.py --pull    # only pull the latest data from GitHub

Needs git with push access to both repos (for example after `gh auth login`).
Set AUTOPUSH=0 to turn it off in run.sh.
"""

import os
import subprocess
import sys
import time
from datetime import datetime

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(BASE_DIR, "data")
DIST_DIR = os.path.join(BASE_DIR, "dist")
EXPORT_REPO = "https://github.com/zynxx24/XII-PPLG---CASH-EXPORTED.git"
DATA_FILES = ["config.json", "members.json", "payments.json", "donations.json", "expenses.json"]

POLL_SECONDS = 5
QUIET_SECONDS = 20  # wait until edits stop for this long, so one session = one commit


def log(msg):
    print(f"[AUTOPUSH {datetime.now():%H:%M:%S}] {msg}", flush=True)


def git(repo, *args, check=True):
    result = subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True)
    if check and result.returncode != 0:
        raise RuntimeError(f"git {' '.join(args)}: {result.stderr.strip() or result.stdout.strip()}")
    return result


def has_changes(repo, *paths):
    return bool(git(repo, "status", "--porcelain", "--", *paths).stdout.strip())


def pull(repo):
    if git(repo, "remote", check=False).stdout.strip():
        git(repo, "pull", "--rebase", "--autostash", "-q")


def ensure_export_repo():
    if not os.path.isdir(os.path.join(DIST_DIR, ".git")):
        if os.path.isdir(DIST_DIR) and os.listdir(DIST_DIR):
            raise RuntimeError("dist/ ada tetapi bukan repo git. Hapus atau pindahkan dulu folder dist/.")
        log("Clone repo export ke dist/ ...")
        subprocess.run(["git", "clone", "-q", EXPORT_REPO, DIST_DIR], check=True)


def commit_and_push(repo, paths, message):
    git(repo, "add", "-A", "--", *paths)
    if not git(repo, "diff", "--cached", "--quiet", check=False).returncode:
        return False
    git(repo, "commit", "-q", "-m", message)
    pull(repo)
    git(repo, "push", "-q")
    return True


def sync():
    stamp = datetime.now().strftime("%Y-%m-%d %H:%M")
    data_paths = [os.path.join("data", name) for name in DATA_FILES]

    if commit_and_push(BASE_DIR, data_paths, f"data: update kas ({stamp})"):
        log("Data terkirim ke Kas-XIIPPLG---cash-management")

    ensure_export_repo()
    pull(DIST_DIR)
    subprocess.run([sys.executable, os.path.join(BASE_DIR, "export_static.py")],
                   check=True, capture_output=True)
    if commit_and_push(DIST_DIR, ["."], f"data: update static export ({stamp})"):
        log("Versi online diperbarui (XII-PPLG---CASH-EXPORTED)")
    else:
        log("Versi online sudah sama, tidak ada yang dikirim")


def data_signature():
    sig = []
    for name in DATA_FILES:
        path = os.path.join(DATA_DIR, name)
        try:
            st = os.stat(path)
            sig.append((name, st.st_mtime_ns, st.st_size))
        except FileNotFoundError:
            sig.append((name, None, None))
    return tuple(sig)


def watch():
    log(f"Mengawasi data/ (kirim {QUIET_SECONDS} detik setelah edit terakhir)")
    last = data_signature()
    changed_at = None
    while True:
        time.sleep(POLL_SECONDS)
        now = data_signature()
        if now != last:
            last, changed_at = now, time.monotonic()
            continue
        if changed_at and time.monotonic() - changed_at >= QUIET_SECONDS:
            changed_at = None
            try:
                sync()
            except Exception as exc:  # keep watching; the next edit retries
                log(f"Gagal push, dicoba lagi di perubahan berikutnya: {exc}")


def main():
    arg = sys.argv[1] if len(sys.argv) > 1 else ""
    if arg == "--pull":
        pull(BASE_DIR)
        log("Data terbaru sudah diambil dari GitHub")
    elif arg == "--watch":
        watch()
    else:
        sync()


if __name__ == "__main__":
    main()
