# VN-Office — Source Code Release (MPL 2.0 / LGPL v3+ compliance)

VN-Office is an office suite for Windows **derived from [LibreOffice](https://www.libreoffice.org/)**,
customized for deployment in Vietnam (Vietnamese UI, locale defaults, Excel
compatibility fixes, Windows 7/8.1 support, custom branding).

This repository publishes the **source-code modifications** that VN-Office
applies on top of upstream LibreOffice, in order to comply with the source
disclosure obligations of the **Mozilla Public License 2.0** (§3.1) and the
**GNU LGPL v3+** under which LibreOffice is licensed.

VN-Office is **not** endorsed by, affiliated with, or certified by The Document
Foundation or the LibreOffice project. "LibreOffice" and "The Document
Foundation" are trademarks of their respective owners and are used here only to
describe the origin of the code, as permitted by the licenses.

---

## Upstream base

These changes apply on top of the upstream LibreOffice `core` repository at
**commit `b19cc5a5d1e0de86ffaa57b6c065bd5776e3d164`**
("comphelper: fix replaceAt string corruption, potential buffer overflow",
by Jonathan Clark, 2026-05-15).

Upstream source: <https://git.libreoffice.org/core> ·
GitHub mirror: <https://github.com/LibreOffice/core>

## What is in this repository

| Path | Contents |
|------|----------|
| `vnoffice-changes.patch` | A single unified diff of every modified file versus the upstream base commit above. Apply with `git apply` / `patch -p1`. |
| `changed-files/` | Full copies of every modified source file (same directory layout as the source tree), for direct inspection without applying the patch. |
| `new-files/` | New source/build files added by VN-Office (build makefile for the bundled font, a dialog `.ui`, a brand image). |
| `COPYING.MPL`, `COPYING.LGPL` | The upstream license texts that govern this code. |
| `NOTICE` | Third-party attribution notice inherited from LibreOffice/Apache OpenOffice. |

### Not included (and why)

* **Build artifacts** (`instdir/`, `workdir/`), logs, and installer binaries —
  not source, not required for disclosure.
* **BeVietnam Pro font** (`.ttf`) — an independent font released by its authors
  under the SIL Open Font License. Obtain it from the official source
  (<https://github.com/bearsan/be-vietnam-pro>) and place it under
  `extras/source/truetype/bevietnampro/`; the build integration file is provided
  in `new-files/extras/Package_bevietnampro.mk`.
* **Internal deployment documents and working files** — not part of the software
  source and out of scope for the license.

## How to build VN-Office from source

1. Clone upstream LibreOffice `core` and check out the base commit:
   ```
   git clone https://github.com/LibreOffice/core.git
   cd core
   git checkout b19cc5a5d1e0de86ffaa57b6c065bd5776e3d164
   ```
2. Apply the VN-Office changes:
   ```
   git apply /path/to/vnoffice-changes.patch
   ```
   (or copy the trees in `changed-files/` and `new-files/` over the checkout).
3. Add the BeVietnam Pro font files (see above).
4. Configure and build following the upstream LibreOffice build instructions for
   Windows (<https://wiki.documentfoundation.org/Development/BuildingOnWindows>),
   using a product name / version of your choosing.

## License

The code in this repository is governed by the same licenses as LibreOffice:
the **Mozilla Public License, v. 2.0** (`COPYING.MPL`) and, for inherited
OpenOffice.org code, the **GNU Lesser General Public License, v. 3 or later**
(`COPYING.LGPL`). Each source file retains its original license header.

See `SOURCE-OFFER.md` for the written offer of source code accompanying binary
distributions of VN-Office.
