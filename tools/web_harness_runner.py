# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

"""Drive the skity web harness in headless Chromium and compare pixels.

The wasm harness (harness/web, target skity-web) renders a small set of
geometry cases through the skity WebGPU backend. This module serves the build
artifacts over a local HTTP server, loads runner.html in headless Chromium
(SwiftShader WebGPU), collects the rendered RGBA buffers and compares them
against the baselines in harness/web/baselines using the same tolerance
semantics as test/golden/common/golden_test_check.cc.

Requires: pip install playwright pillow (plus `python -m playwright install
chromium`).
"""

import base64
import json
import os
import re
import threading
import time
from http.server import HTTPServer, SimpleHTTPRequestHandler
from pathlib import Path

# Same tolerance semantics as golden_test_check.cc.
IGNORED_CHANNEL_DIFF = 2
IGNORED_SOFT_EDGE_CHANNEL_DIFF = 8
MAX_DIFF_PERCENT = 0.1  # rendered as a 0..1 ratio, i.e. 10%
MAX_MAX_DIFF_PERCENT = 0.5  # 50% of a channel
MAX_DIFF_PIXEL_COUNT = 50

STAGE_VISUAL = "Layer 3: Visual Regression"


def add_arguments(parser):
    parser.add_argument("--web-browser-timeout", type=int, default=120000,
                        help="Milliseconds to wait for the browser run (default: 120000)")
    parser.add_argument("--web-headed", action="store_true",
                        help="Show the browser window (local debugging only)")


def _to_snake(name):
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def _infra_report(runner, reason_code, message):
    return {
        "summary": {"total": 0, "passed": 0, "failed": 0,
                    "duration_ms": int((time.time() - runner.started) * 1000)},
        "failures": [{
            "suite": "web-harness",
            "case_name": "Infrastructure",
            "status": "FAIL",
            "exit_code": 2,
            "stage": "Layer 0: Infrastructure",
            "reason_code": reason_code,
            "backend": "webgpu",
            "oracle": "system",
            "error": message,
            "artifacts": {},
        }],
        "results": [],
    }


def _premul_to_unpremul(data, width, height):
    out = bytearray(len(data))
    for off in range(0, len(data), 4):
        alpha = data[off + 3]
        if alpha == 0:
            continue
        for i in range(3):
            out[off + i] = min(255, (data[off + i] * 255 + alpha // 2) // alpha)
        out[off + 3] = alpha
    return bytes(out)


def _save_rgba_png(pil_image, premul_data, width, height, path):
    pil_image.frombytes("RGBA", (width, height),
                        _premul_to_unpremul(premul_data, width, height)).save(path)


def _comparable(baseline_rgba, offset, index, alpha):
    # Baselines are straight-alpha PNGs; fold RGB back into premultiplied
    # space before comparing, mirroring GetComparableChannel.
    if index < 3:
        return (baseline_rgba[offset + index] * alpha + 127) // 255
    return baseline_rgba[offset + 3]


def _compare_pixels(actual_premul, baseline_rgba, width, height):
    """Replicates ComparePixelsImpl of golden_test_check.cc."""
    total = width * height
    diff_pixel_count = 0
    diff_percent = 0.0
    max_diff_percent = 0.0
    diff_pixels = []
    min_x, min_y, max_x, max_y = width, height, -1, -1

    for y in range(height):
        row = y * width * 4
        for x in range(width):
            off = row + x * 4
            src_alpha = actual_premul[off + 3]
            dst_alpha = baseline_rgba[off + 3]
            pixel_diff = False
            pixel_max = 0.0
            for i in range(4):
                src_channel = actual_premul[off + i]
                dst_channel = _comparable(baseline_rgba, off, i, dst_alpha)
                diff = abs(src_channel - dst_channel)
                if diff > IGNORED_CHANNEL_DIFF:
                    pixel_max = max(pixel_max, diff / 255.0)
                    pixel_diff = True

            if pixel_diff and (0 < src_alpha < 255 or 0 < dst_alpha < 255):
                soft_diff = False
                for i in range(4):
                    src_channel = actual_premul[off + i]
                    dst_channel = _comparable(baseline_rgba, off, i, dst_alpha)
                    if abs(src_channel - dst_channel) > IGNORED_SOFT_EDGE_CHANNEL_DIFF:
                        soft_diff = True
                        break
                if not soft_diff:
                    pixel_diff = False
                    pixel_max = 0.0

            if pixel_diff:
                diff_pixel_count += 1
                diff_percent += 1.0 / total
                max_diff_percent = max(max_diff_percent, pixel_max)
                min_x, min_y = min(min_x, x), min(min_y, y)
                max_x, max_y = max(max_x, x), max(max_y, y)
                if len(diff_pixels) < 1000:
                    diff_pixels.append({
                        "x": x, "y": y,
                        "src": list(actual_premul[off:off + 4]),
                        "dst": list(baseline_rgba[off:off + 4]),
                    })

    passed = (diff_percent <= MAX_DIFF_PERCENT and
              max_diff_percent <= MAX_MAX_DIFF_PERCENT and
              diff_pixel_count <= MAX_DIFF_PIXEL_COUNT)

    metrics = {
        "diff_percent": diff_percent,
        "max_diff_percent": max_diff_percent,
        "diff_pixel_count": diff_pixel_count,
    }
    if diff_pixel_count > 0:
        metrics["diff_bbox"] = [min_x, min_y,
                                max_x - min_x + 1, max_y - min_y + 1]
    return passed, metrics, diff_pixels


def run_suite(runner, args):
    repo = Path(runner.repo_root).resolve()
    harness_build_dir = Path(runner.build_dir).resolve() / "harness" / "web"
    baselines_dir = repo / "harness" / "web" / "baselines"
    failures_dir = Path(runner.golden_failures_dir)
    update_missing = os.environ.get("SKITY_UPDATE_MISSING_GOLDEN", "").lower() in (
        "1", "true", "yes")
    name_filter = (args.filter or "").strip()

    if not (harness_build_dir / "skity-web.js").is_file():
        return _infra_report(runner, "missing_executable",
                             "skity-web.js not found under "
                             f"{harness_build_dir}; build target skity-web "
                             "with an Emscripten toolchain")

    try:
        from playwright.sync_api import sync_playwright
    except ImportError:
        return _infra_report(runner, "missing_runtime_dependency",
                             "playwright is not installed: pip install playwright")
    try:
        from PIL import Image
    except ImportError:
        return _infra_report(runner, "missing_runtime_dependency",
                             "Pillow is not installed: pip install pillow")

    def handler(*cmd_args, **kwargs):
        return SimpleHTTPRequestHandler(*cmd_args, directory=str(harness_build_dir),
                                        **kwargs)

    server = HTTPServer(("127.0.0.1", 0), handler)
    port = server.server_address[1]
    threading.Thread(target=server.serve_forever, daemon=True).start()

    console_errors = []
    results = []
    fatal = None
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(
                headless=not args.web_headed,
                args=["--enable-unsafe-webgpu", "--enable-unsafe-swiftshader",
                      "--no-sandbox"])
            try:
                page = browser.new_page()
                page.on("console", lambda message:
                        console_errors.append(message.text)
                        if message.type == "error" else None)
                page.on("pageerror", lambda error: console_errors.append(str(error)))
                page.goto(f"http://127.0.0.1:{port}/runner.html")
                page.wait_for_function("window.__harnessDone === true",
                                       timeout=args.web_browser_timeout)
                fatal = page.evaluate("window.__harnessFatal")
                results = page.evaluate("window.__harnessResults") or []
            finally:
                browser.close()
    except Exception as error:  # noqa: BLE001 - surfaced as infra failure below
        console_text = "\n".join(console_errors[-20:])
        return _infra_report(runner, "runtime_crash",
                             f"browser run failed: {error}\nconsole:\n{console_text}")
    finally:
        server.shutdown()

    if fatal:
        console_text = "\n".join(console_errors[-20:])
        return _infra_report(runner, "runtime_crash",
                             f"runner.js fatal: {fatal}\nconsole:\n{console_text}")
    if not results:
        return _infra_report(runner, "runtime_crash",
                             "runner.js produced no results")

    started = time.time()
    failures = []
    records = []
    for item in results:
        name = item["name"]
        if name_filter and name_filter not in name:
            continue
        width, height = item["width"], item["height"]
        record = {"suite": "web-harness", "case_name": name, "backend": "webgpu",
                  "status": "PASS", "exit_code": 0, "stage": STAGE_VISUAL,
                  "reason_code": "pass", "error": ""}
        artifacts = {}
        metrics = {}

        baseline_path = baselines_dir / (_to_snake(name) + ".png")
        if not item["ok"]:
            record.update(status="FAIL", exit_code=1, reason_code="runtime_error",
                          error=item["error"] or "case failed in the browser")
        elif not baseline_path.is_file():
            if update_missing:
                baselines_dir.mkdir(parents=True, exist_ok=True)
                _save_rgba_png(Image, base64.b64decode(item["pixels"]),
                               width, height, baseline_path)
                record["reason_code"] = "baseline_written"
            else:
                record.update(status="FAIL", exit_code=1,
                              reason_code="missing_baseline",
                              error=f"Missing baseline {baseline_path}; set "
                                    "SKITY_UPDATE_MISSING_GOLDEN=1 to create it")
        else:
            actual = base64.b64decode(item["pixels"])
            with Image.open(baseline_path) as baseline_image:
                baseline_image = baseline_image.convert("RGBA")
                if baseline_image.size != (width, height):
                    record.update(status="FAIL", exit_code=1,
                                  reason_code="pixel_mismatch",
                                  error=f"baseline size {baseline_image.size} does "
                                        f"not match case size {(width, height)}")
                    baseline_rgba = None
                else:
                    baseline_rgba = baseline_image.tobytes()

            if baseline_rgba is not None:
                passed, metrics, diff_pixels = _compare_pixels(
                    actual, baseline_rgba, width, height)
                if not passed:
                    failures_dir.mkdir(parents=True, exist_ok=True)
                    actual_png = failures_dir / f"{_to_snake(name)}_actual.png"
                    diff_json = failures_dir / f"{_to_snake(name)}_diff_pixels.json"
                    _save_rgba_png(Image, actual, width, height, actual_png)
                    diff_json.write_text(json.dumps(diff_pixels))
                    artifacts = {"expected_image": str(baseline_path),
                                 "actual_image": str(actual_png),
                                 "diff_pixels_file": str(diff_json)}
                    record.update(status="FAIL", exit_code=1,
                                  reason_code="pixel_mismatch",
                                  error="rendered pixels differ from the baseline")
                    record["metrics"] = metrics
                    record["artifacts"] = artifacts

        if record["status"] == "FAIL":
            failures.append(record)
        records.append(record)

    report = {
        "summary": {"total": len(records),
                    "passed": len(records) - len(failures),
                    "failed": len(failures),
                    "duration_ms": int((time.time() - started) * 1000)},
        "failures": failures,
        "results": records,
    }
    return report
