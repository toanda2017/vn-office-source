# VN-Office extras (re-apply into instdir after each full build)

A full `make` regenerates instdir and wipes these runtime additions. After
building, copy them back before packaging the installer:

- `basic/VNLang/*` → `instdir/share/basic/VNLang/`  (language-switch macro)
  and register VNLang in `instdir/share/basic/script.xlc`.
- `registry/Addons.xcu` → `instdir/share/registry/data/org/openoffice/Office/Addons.xcu`
  (adds the "Ngôn ngữ / Language" menu calling the macro).
- `registry/Linguistic.xcu` → same registry data folder (sets UILocale=vi-VN;
  also baked into source Linguistic.xcs schema, so this loose file is a safety copy).

Also refresh: `icon-themes/colibre/brand/shell/logo-sc*.svg` and `VNOFFICE-SOURCE-OFFER.txt`.
