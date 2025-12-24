# License

## MIT License

Copyright (c) GlassOnTin 2025 

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

---

## Provenance and Third-Party Licenses

This project implements a chorded keyboard using original hardware and firmware designs. The chord layouts used are derived from openly licensed community projects and are not proprietary to any commercial product.

### Project Components

**Original work (MIT License):**
- Hardware design (schematic, PCB layout)
- Firmware implementation
- Documentation

**Third-party chord layouts:**
The following chord layouts may be included or referenced. Each retains its original license:

| Layout | Author | License | Source |
|--------|--------|---------|--------|
| MirrorWalk | Griatch (based on Bill Horner's walking layout) | BSD-3-Clause | [github.com/Griatch/twiddler-configs](https://github.com/Griatch/twiddler-configs) |
| Walking Layout | Bill Horner | Public Domain / Unlicensed | [github.com/ben-horner/twiddler_layout](https://github.com/ben-horner/twiddler_layout) |
| TabSpace | Brandon Rhodes (1999) | Public Domain | [rhodesmill.org](http://rhodesmill.org/brandon/projects/tabspace-guide.pdf) |
| BackSpice | Alex Bravo | Unlicensed | [github.com/AlexBravo/Twiddler](https://github.com/AlexBravo/Twiddler) |
| CoolHand | CoohLand | Unlicensed | [github.com/CoohLand/CoolHand](https://github.com/CoohLand/CoolHand) |

### BSD-3-Clause License (for MirrorWalk layout)

```
Copyright (c) Griatch

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```

---

## Legal Background

### What This Project Is

This is an independent, clean-room implementation of a chorded keyboard. The concept of chording—pressing multiple keys simultaneously to produce characters—has been in the public domain since the 1960s (notably Doug Engelbart's work at SRI International).

The chord layouts used are community-created optimisations based on:
- Letter frequency analysis in English text
- N-gram transition calculations for ergonomic "walking" between chords
- User preference and experimentation

These are mathematical derivations and user interface designs, neither of which are protectable by copyright. The specific implementations (config files, cheat sheets) are licensed by their creators as noted above.

### Hardware

The hardware uses commodity, off-the-shelf components:
- Nordic nRF52840 microcontroller (ARM Cortex-M4F @ 64MHz)
- EByte E73-2G4M08S1C module
- Standard tactile switches connected to GPIO

No proprietary hardware designs have been copied or reverse engineered.

### Firmware

The firmware is an original implementation using:
- Nordic nRF5 SDK (licensed by Nordic Semiconductor)
- Standard USB HID protocol (public specification)
- Standard Bluetooth Low Energy HID profile (public specification)

### What This Project Is Not

This project does not contain:
- Firmware extracted from any commercial product
- Copied source code from any commercial product
- Proprietary chord mappings from any commercial product
- Circumvention of any technological protection measures

---

## Trademark Notice

"Twiddler" is a trademark of Tek Gear Inc. This project is not affiliated with, endorsed by, or sponsored by Tek Gear Inc. or any of its affiliates.

References to "Twiddler" hardware in this project are for compatibility description purposes only under nominative fair use principles.

---

## Disclaimer

This legal summary is provided for informational purposes only and does not constitute legal advice. If you have concerns about the legality of using or contributing to this project, consult a qualified legal professional in your jurisdiction.
