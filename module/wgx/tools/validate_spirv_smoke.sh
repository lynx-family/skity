#!/usr/bin/env bash
# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

set -euo pipefail

BUILD_DIR="${1:-out/cmake_host_build}"
TEST_BIN="$BUILD_DIR/test/ut/skity_unit_test"
SPV_DIR="${WGX_SPIRV_DUMP_DIR:-$BUILD_DIR}"
VAL_BIN="${SPIRV_VAL_BIN:-/usr/local/bin/spirv-val}"
DIS_BIN="${SPIRV_DIS_BIN:-/usr/local/bin/spirv-dis}"
TEST_FILTER="${WGX_GTEST_FILTER:-WgxSpirvSmokeTest.*}"

if [[ ! -x "$TEST_BIN" ]]; then
  echo "error: test binary not found or not executable: $TEST_BIN" >&2
  exit 1
fi

if [[ ! -x "$VAL_BIN" ]]; then
  echo "error: spirv-val not found or not executable: $VAL_BIN" >&2
  exit 1
fi

if [[ ! -x "$DIS_BIN" ]]; then
  echo "error: spirv-dis not found or not executable: $DIS_BIN" >&2
  exit 1
fi

echo "== Running smoke tests =="
WGX_SPIRV_DUMP_DIR="$SPV_DIR" "$TEST_BIN" --gtest_filter="$TEST_FILTER"

shopt -s nullglob
spv_files=("$SPV_DIR"/*.spv)
shopt -u nullglob

if [[ ${#spv_files[@]} -eq 0 ]]; then
  echo "error: no .spv files were generated in $SPV_DIR" >&2
  exit 1
fi

echo "== Validating SPIR-V with spirv-val =="
for f in "${spv_files[@]}"; do
  base="$(basename "$f")"
  if [[ "$base" == "wgx_vs_main_workgroup.spv" ]]; then
    echo "[spirv-val] skip $f (workgroup storage is not supported for vertex entry points)"
    continue
  fi
  echo "[spirv-val] $f"
  "$VAL_BIN" --target-env vulkan1.1 "$f"
done

echo "== Disassembling SPIR-V with spirv-dis =="
for f in "${spv_files[@]}"; do
  out="${f%.spv}.spvasm"
  echo "[spirv-dis] $f -> $out"
  "$DIS_BIN" "$f" -o "$out"
done

echo "Done: smoke tests, spirv-val, and spirv-dis completed successfully."
