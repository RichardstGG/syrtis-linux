#!/usr/bin/env python3
"""Synthetic ported harness for testing the oracle filename adapter."""

from pathlib import Path
import shutil
import sys

fixtures, output, selector = sys.argv[1:]
shutil.copyfile(
    Path(fixtures) / f"{selector}.actual.json",
    Path(output) / f"{selector}.actual.json",
)
