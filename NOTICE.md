# Notices

RING is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See [LICENSE](LICENSE).

Copyright (C) 2026 Yaniv Schonfeld (willbearfruits).

## What RING carries besides its own code

- **Terminus Font**, the text on screen (its bitmaps from 12 to 32 pixels high, turned into C as `core/font_data.c` by the R36S
  rhythm machine's `tools/mkfont.py`). Copyright (C) 2020 Dimitar Toshkov Zhekov. SIL Open Font License 1.1:
  [LICENSES/Terminus-Font-OFL-1.1.txt](LICENSES/Terminus-Font-OFL-1.1.txt).

The Raspberry Pi image that `make image` builds is Raspberry Pi OS Lite with RING added; the operating system and its
packages keep their own licences, which the image carries in `/usr/share/doc`.

## Written from others' work

- **Ableton Link**: `core/link.c` is BARE!'s implementation, ported. BARE!'s was written by reading Ableton's own
  source (github.com/Ableton/link, GPL-2.0-or-later) and checked against it. Ableton and Link are trademarks of
  Ableton AG; RING is not made or endorsed by Ableton.
- **Key finding**: HEAR KEY compares the input's chroma with the major and minor key profiles published by Carol L.
  Krumhansl and Edward J. Kessler (1982). The numbers are theirs; the code is RING's.

## From the same maker's other projects

Written by the same author and carried over under RING's licence:

- [BARE!](https://github.com/willbearfruits/bare): Ableton Link (`core/link.c`, `core/link.h`).
- The R36S rhythm machine ([r36s-rhythm](https://github.com/willbearfruits/r36s-rhythm)): drawing, fonts, the small
  library and the DSP blocks (`core/gfx.*`, `core/font*`, `core/lib.*`, `core/dsp.h`; `dsp.h` came there from thump,
  a drum synthesizer).
- PSX DRUMS, a PlayStation drum machine: the recipes of the twelve drums (`core/drums.c`), and the loops `make test`
  checks the follower against.
- keys: the ALSA MIDI host (`host/midi_alsa.c`).

PlayStation is a trademark of Sony Interactive Entertainment; nothing here is made or endorsed by Sony.
