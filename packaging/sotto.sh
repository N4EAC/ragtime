# The sotto vocoder Ragtime's voice messages are coded with: codec 2 is
# sotto v3.8 (docs/PROTOCOL.md, "Voice"), 16 kHz speech with the 4-8 kHz
# hiss band, and the same library plays codec 1 (v3.6's frames) at 8 kHz.
# It also has sotto 1200, 600 and 400 with the hiss band, float, both
# ways (sotto_lr_enc16 / _dec16, sotto_vlr_enc16 / _dec16): codecs 3, 4
# and 5. This pin includes the creak and pitch-line fixes for low rates,
# the peak-weighted v3.8 shape search, and the short pitch-run fold.
# Frame layouts and the default decoder delay remain compatible.
# Ragtime needs a library whose sotto_format() says "sotto v3.8, shape
# 9-9-8-8", so the variant is named here rather than left to sotto's
# Makefile. From this commit sotto's generated C tables are committed, so
# the build needs no Python. Ragtime loads libsotto at run time and never
# links it (LGPL through Codec 2); the packages carry it beside ragtime with
# its licences. Sourced by build-sotto.sh.
SOTTO_REPO=${SOTTO_REPO:-https://github.com/baitisj/sotto.git}
SOTTO_COMMIT=6686fe4f783d7bdf9d96cd1177aa59155e18b833
SOTTO_VARIANT=v3.8
