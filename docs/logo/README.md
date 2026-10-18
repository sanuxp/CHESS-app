# Logo

A pawn casting the shadow of a queen: the idea of promotion.

| File | Use |
|---|---|
| `banner-gold.jpg` | 1000 px version of the gold artwork (about 100 KB). Too detailed for small icons, so used for the README banner and splash screens. |
| `banner-halloween.jpg` | 1000 px version of the Halloween artwork. Seasonal banner only. |
| `logo-simple.svg` | **The app icon.** Simplified flat vector of the same idea; stays crisp down to 16 px. |
| `logo-halloween.svg` | Seasonal edition of the icon (harvest moon, bats). |
| `proposals.png` | The comparison sheet the design was chosen from. |
| `halloween-preview.png` | Preview of the Halloween icon at several sizes. |

The 2 MB full-size originals (`source/`), the generated PNGs (`final/`) and the sheet's HTML source are
kept out of git (see `.gitignore`); the generated PNGs are recreated by the script, and copies of the ones the
Flutter tool needs live in `flutter/assets/icon/`.

## Where the icon is used
- **C++ app**: `cpp/app/res/chess.ico` (7 sizes, 16-256 px), embedded in `chess.exe` by `cpp/app/res/chess.rc`.
  SDL shows it in the title bar and taskbar; Explorer shows it on the file.
- **Flutter app**: generated for Android (legacy + adaptive), iOS, web (favicon and PWA icons) and Windows
  by `flutter_launcher_icons`, configured by `flutter/flutter_launcher_icons.yaml`.

## Changing the icon or switching to the Halloween edition
```powershell
python tools/make_icons.py halloween     # or: python tools/make_icons.py   (back to the default gold logo)
cd flutter
dart run flutter_launcher_icons          # rewrites the Android / iOS / web / Windows icons
cd ..
powershell -ExecutionPolicy Bypass -File cpp\build.ps1     # rebuild chess.exe with the new .ico
```
Then rebuild the Flutter app (`flutter build windows`, `flutter build apk` ...). The script needs Pillow
(`pip install pillow`) and Chrome or Edge (used only to turn the SVG into a PNG). To change the design,
edit the SVG and run the same commands.
