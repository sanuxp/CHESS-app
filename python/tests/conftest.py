import json
import os
import sys
from pathlib import Path

os.environ.setdefault("SDL_VIDEODRIVER", "dummy")  # the UI tests run without a window
os.environ.setdefault("SDL_AUDIODRIVER", "dummy")
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

TESTDATA = ROOT.parent / "docs" / "testdata"


def load_json(name):
    return json.loads((TESTDATA / name).read_text(encoding="utf-8"))
