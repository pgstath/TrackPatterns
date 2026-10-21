"""TRACK PATTERNS on the stock effects: the first test image.

Copy the trackpatterns/ directory to sdk/octabam/modules/trackpatterns/ and
this file to sdk/octabam/remixes/trackpatterns-test/remix.py, then from
sdk/octabam:

    python3 modules/trackpatterns/generate.py
    make bus REMIX=trackpatterns-test

No DSP module: both FX choosers stay stock's. static_stock keeps the stock
DSP code built in.
"""
from remix.schema import Remix

REMIX = Remix(
    name="trackpatterns-test",
    doc="TRACK PATTERNS (a pattern per track) on the stock effects",
    modules=("TRACK PATTERNS",
             "FILTER", "EQUALIZER", "DJ EQ", "PHASER", "FLANGER", "CHORUS",
             "SPATIALIZER", "COMB FILTER", "COMPRESSOR", "LO-FI", "DELAY",
             "PLATE REV", "SPRING REV", "DARK REV"),
    fallback="NONE",
    static_stock=True,
)
