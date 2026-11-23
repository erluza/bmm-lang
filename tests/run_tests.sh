#!/bin/sh
set -e
cd "$(dirname "$0")/.."
python3 tests/run_tests.py
