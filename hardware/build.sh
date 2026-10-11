#!/bin/bash
# rebuild every printable file from the designs, run the checks, fill print/
# needs: openscad (2025+ with manifold), python3 with requirements.txt,
# and the original Mochi housing STLs in original/ (see README)
set -e
cd "$(dirname "$0")"
PY=${PY:-python3}
scad() { openscad --backend=manifold -o "$@" 2>&1 | grep -E "ERROR" && exit 1 || true; }

[ -f "original/Housing-Body.stl" ] || { echo "original/ is missing, download the Mochi housing first (README)"; exit 1; }
mkdir -p out

echo "housings"
scad out/mac_housing.stl mac_housing.scad
scad out/mac_back_plate.stl mac_back_plate.scad
scad out/clawd_v1_housing.stl clawd_v1_housing.scad
scad out/clawd_ball_body_full.stl -D ball_in_body=true clawd_v1_housing.scad

echo "football"
$PY football_badge.py
for c in red black white; do scad out/clawd_ball_body_$c.stl -D "part=\"$c\"" ball_in_body.scad; done

echo "tests"
scad out/tolerance_rings.stl tolerance_rings.scad
scad out/clawd_housing_fit_test.stl clawd_fit_test.scad

echo "stands + checks"
$PY check_stands.py --render
$PY check_fit.py out/mac_housing.stl out/mac_back_plate.stl
$PY check_fit.py out/clawd_ball_body_full.stl "original/Back Plate-Body.stl"

echo "print files"
$PY print_3mf.py
