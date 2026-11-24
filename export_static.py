#!/usr/bin/env python3
"""
Build the read-only static copy of the cash dashboard into dist/.

dist/ is a separate git repo (XII-PPLG---CASH-EXPORTED) served by GitHub Pages.
Everything in dist/ is regenerated except the files listed in KEEP, which live
only in the export repo (its README, screenshots, git metadata).

    python3 export_static.py
"""

import json
import os
import re
import shutil

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
PUBLIC_DIR = os.path.join(BASE_DIR, "public")
DATA_DIR = os.path.join(BASE_DIR, "data")
DIST_DIR = os.path.join(BASE_DIR, "dist")

KEEP = {".git", "README.md", "docs", ".nojekyll"}
DATA_FILES = ["config.json", "members.json", "payments.json", "donations.json", "expenses.json"]

SITE_URL = "https://zynxx24.github.io/XII-PPLG---CASH-EXPORTED/"
TITLE = "Kas XII PPLG | SMKN 2 Kuta Selatan"
DESCRIPTION = ("Dashboard kas kelas XII PPLG SMKN 2 Kuta Selatan: saldo, pemasukan, "
               "pengeluaran, donasi, dan status kas per siswa.")
FAVICON = ("data:image/svg+xml,%3Csvg%20xmlns%3D%27http%3A//www.w3.org/2000/svg%27%20viewBox%3D%270%200%2064%2064%27%3E"
           "%3Crect%20width%3D%2764%27%20height%3D%2764%27%20rx%3D%2714%27%20fill%3D%27%234f46e5%27/%3E"
           "%3Ctext%20x%3D%2732%27%20y%3D%2744%27%20font-family%3D%27Arial%2Csans-serif%27%20font-size%3D%2734%27"
           "%20font-weight%3D%27700%27%20fill%3D%27%23fff%27%20text-anchor%3D%27middle%27%3EK%3C/text%3E%3C/svg%3E")

# noindex: the page lists students by name with their unpaid dues.
HEAD_META = f"""<title>{TITLE}</title>
    <meta name="description" content="{DESCRIPTION}">
    <link rel="canonical" href="{SITE_URL}">
    <link rel="icon" href="{FAVICON}">
    <meta property="og:type" content="website">
    <meta property="og:locale" content="id_ID">
    <meta property="og:title" content="{TITLE}">
    <meta property="og:description" content="{DESCRIPTION}">
    <meta property="og:url" content="{SITE_URL}">
    <meta property="og:image" content="{SITE_URL}docs/og-image.jpg">
    <meta property="og:image:width" content="1200">
    <meta property="og:image:height" content="630">
    <meta name="twitter:card" content="summary_large_image">
    <meta name="robots" content="noindex, nofollow">"""


def load_json(name):
    with open(os.path.join(DATA_DIR, name), encoding="utf-8") as f:
        return json.load(f)


def clean_dist():
    os.makedirs(DIST_DIR, exist_ok=True)
    for item in os.listdir(DIST_DIR):
        if item in KEEP:
            continue
        path = os.path.join(DIST_DIR, item)
        shutil.rmtree(path) if os.path.isdir(path) else os.remove(path)
    open(os.path.join(DIST_DIR, ".nojekyll"), "w").close()


def with_head_meta(html):
    html = re.sub(r"\s*<meta name=\"description\"[^>]*>", "", html)
    return re.sub(r"<title>.*?</title>", lambda _: HEAD_META, html, count=1, flags=re.S)


def export_static():
    clean_dist()

    for folder, src in (("css", PUBLIC_DIR), ("js", PUBLIC_DIR), ("asset", BASE_DIR)):
        if os.path.isdir(os.path.join(src, folder)):
            shutil.copytree(os.path.join(src, folder), os.path.join(DIST_DIR, folder))

    os.makedirs(os.path.join(DIST_DIR, "data"), exist_ok=True)
    embedded = {}
    for name in DATA_FILES:
        if os.path.exists(os.path.join(DATA_DIR, name)):
            shutil.copy2(os.path.join(DATA_DIR, name), os.path.join(DIST_DIR, "data", name))
            embedded[name[:-5]] = load_json(name)

    with open(os.path.join(DIST_DIR, "js", "static_data.js"), "w", encoding="utf-8") as f:
        f.write(f"window._embeddedData = {json.dumps(embedded, indent=2, ensure_ascii=False)};\n")

    with open(os.path.join(PUBLIC_DIR, "dashboard.html"), encoding="utf-8") as f:
        html = f.read()
    html = html.replace('<script src="js/api.js"></script>',
                        '<script src="js/static_data.js"></script>\n    <script src="js/api.js"></script>')
    with open(os.path.join(DIST_DIR, "index.html"), "w", encoding="utf-8") as f:
        f.write(with_head_meta(html))

    with open(os.path.join(PUBLIC_DIR, "admin.html"), encoding="utf-8") as f:
        admin = f.read().replace("/dashboard.html", "index.html")
    with open(os.path.join(DIST_DIR, "admin.html"), "w", encoding="utf-8") as f:
        f.write(admin)

    print(f"Export selesai: {DIST_DIR}")


if __name__ == "__main__":
    export_static()
