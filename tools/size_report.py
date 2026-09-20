#!/usr/bin/env python3
# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
"""Collect and compare native library sizes of the android aar for CI.

Subcommands:
  collect  measure jni/*/libskity.so entries inside a release aar and emit
           size-data.json plus per-abi nm dumps for later symbol diffs
  compare  diff current sizes against a baseline snapshot, render the PR
           comment markdown and emit threshold warning annotations
  trend    append a data point to the size-data branch and regenerate the
           mermaid trend README on that branch
"""

import argparse
import glob
import gzip
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from datetime import datetime, timezone

MARKER = '<!-- skity-size-report -->'

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def Die(message):
    sys.exit('size_report: {}'.format(message))


def Run(cmd):
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        Die('command failed: {}\nstdout: {}\nstderr: {}'.format(
            ' '.join(cmd), result.stdout, result.stderr))
    return result.stdout


def FindTool(preferred, fallback):
    local = os.path.join(REPO_ROOT, 'buildtools', 'llvm', 'bin', preferred)
    if os.path.isfile(local):
        return local
    return shutil.which(preferred) or shutil.which(fallback)


def LoadJson(path):
    with open(path) as handle:
        return json.load(handle)


def WriteText(path, text):
    with open(path, 'w') as handle:
        handle.write(text)


def FmtBytes(value):
    size = abs(float(value))
    sign = '-' if value < 0 else ''
    if size >= 1024 * 1024:
        return '{}{:.2f} MB'.format(sign, size / 1024 / 1024)
    if size >= 1024:
        return '{}{:.1f} KB'.format(sign, size / 1024)
    return '{}{} B'.format(sign, int(round(size)))


def FmtDelta(value):
    return ('+' if value > 0 else '') + FmtBytes(value)


def Esc(text):
    return text.replace('|', '\\|').replace('`', "'").replace('\n', ' ')


def RunNm(so_path):
    tool = FindTool('llvm-nm', 'nm')
    if not tool:
        Die('no llvm-nm/nm found on PATH')
    output = Run([tool, '-S', '--size-sort', '--defined-only', so_path])
    if not output.strip():
        # release .so shipped in the aar is stripped: its static symbol
        # table is gone, fall back to the dynamic symbol table
        output = Run([tool, '-D', '-S', '--size-sort', '--defined-only',
                      so_path])
    return output


def ParseNm(path):
    symbols = {}
    if not os.path.isfile(path):
        return symbols
    with open(path) as handle:
        for line in handle:
            parts = line.rstrip('\n').split(maxsplit=3)
            if len(parts) != 4:
                continue
            _addr, size_hex, sym_type, name = parts
            try:
                size = int(size_hex, 16)
            except ValueError:
                continue
            if name in symbols:
                symbols[name][1] += size
            else:
                symbols[name] = [sym_type, size]
    return symbols


def Demangle(names):
    tool = FindTool('llvm-cxxfilt', 'c++filt')
    if not tool or not names:
        return {name: name for name in names}
    try:
        result = subprocess.run([tool], input='\n'.join(names),
                                capture_output=True, text=True, check=True)
        lines = result.stdout.rstrip('\n').split('\n')
        if len(lines) == len(names):
            return dict(zip(names, lines))
    except subprocess.CalledProcessError:
        pass
    return {name: name for name in names}


def RunSections(so_path):
    tool = FindTool('llvm-size', 'size')
    if not tool:
        return {}
    result = subprocess.run([tool, '-B', so_path], capture_output=True,
                            text=True)
    lines = [line for line in result.stdout.splitlines() if line.strip()]
    if len(lines) < 2:
        return {}
    fields = lines[1].split()
    try:
        return {'text': int(fields[0]), 'data': int(fields[1]),
                'bss': int(fields[2])}
    except (IndexError, ValueError):
        return {}


def CmdCollect(args):
    aars = sorted(glob.glob(args.aar_glob, recursive=True))
    if len(aars) != 1:
        Die('expected exactly one aar for glob {!r}, got {}'.format(
            args.aar_glob, aars))
    os.makedirs(args.out, exist_ok=True)
    artifacts = []
    with zipfile.ZipFile(aars[0]) as archive:
        for info in archive.infolist():
            match = re.match(r'^jni/([^/]+)/libskity\.so$', info.filename)
            if not match:
                continue
            abi = match.group(1)
            so_bytes = archive.read(info)
            with tempfile.TemporaryDirectory() as tmp:
                so_path = os.path.join(tmp, 'libskity.so')
                with open(so_path, 'wb') as handle:
                    handle.write(so_bytes)
                nm_dump = RunNm(so_path)
                sections = RunSections(so_path)
            nm_file = '{}.nm.txt'.format(abi)
            WriteText(os.path.join(args.out, nm_file), nm_dump)
            gzip_size = len(gzip.compress(so_bytes, 9, mtime=0))
            if info.compress_size and info.compress_size < info.file_size:
                compressed, basis = info.compress_size, 'aar-deflate'
            else:
                # the aar stored the entry uncompressed; fall back to a
                # self-consistent gzip metric instead
                compressed, basis = gzip_size, 'gzip-9'
            artifacts.append({
                'name': 'libskity.so',
                'abi': abi,
                'size': info.file_size,
                'compressed': compressed,
                'compressed_basis': basis,
                'gzip9': gzip_size,
                'sections': sections,
                'nm': nm_file,
            })
            print('collected {}: size={} compressed={} ({})'.format(
                abi, FmtBytes(info.file_size), FmtBytes(compressed), basis))
    if not artifacts:
        Die('no jni/*/libskity.so entries found inside {}'.format(aars[0]))
    artifacts.sort(key=lambda item: item['abi'])
    data = {
        'schema': 1,
        'created_at': datetime.now(timezone.utc).isoformat(timespec='seconds'),
        'event': args.event or '',
        'head_sha': args.head_sha or '',
        'pr_number': int(args.pr_number) if args.pr_number else None,
        'aar': {'path': aars[0], 'size': os.path.getsize(aars[0])},
        'artifacts': artifacts,
    }
    out_path = os.path.join(args.out, 'size-data.json')
    WriteText(out_path, json.dumps(data, indent=2) + '\n')
    print('wrote {}'.format(out_path))


def Percent(delta, base):
    if not base:
        return None
    return delta / float(base) * 100.0


def Verdict(delta, pct, max_pct):
    if pct is None:
        return '⚪'
    if pct > max_pct:
        return '🔴'
    if delta < 0:
        return '🟢'
    return '⚪'


def SymbolDiff(cur_symbols, base_symbols, top):
    deltas = []
    for name in set(cur_symbols) | set(base_symbols):
        cur = cur_symbols.get(name)
        base = base_symbols.get(name)
        cur_size = cur[1] if cur else 0
        base_size = base[1] if base else 0
        delta = cur_size - base_size
        if delta == 0:
            continue
        deltas.append({
            'name': name,
            'type': (cur or base)[0],
            'delta': delta,
            'cur': cur_size,
            'base': base_size,
        })
    grown = sorted((d for d in deltas if d['delta'] > 0),
                   key=lambda d: -d['delta'])[:top]
    shrunk = sorted((d for d in deltas if d['delta'] < 0),
                    key=lambda d: d['delta'])[:top]
    return grown, shrunk


def BuildRows(current, baseline, max_pct):
    rows = []
    for cur in current['artifacts']:
        base = None
        if baseline:
            base = next((item for item in baseline['artifacts']
                         if item['abi'] == cur['abi']), None)
        for metric in ('size', 'compressed'):
            if metric == 'size':
                label = 'Uncompressed'
            else:
                label = 'Compressed ({})'.format(
                    cur.get('compressed_basis', ''))
            base_value = base[metric] if base else None
            cur_value = cur[metric]
            delta = cur_value - base_value if base_value is not None else None
            pct = Percent(delta, base_value) if delta is not None else None
            rows.append({
                'abi': cur['abi'],
                'label': label,
                'base': base_value,
                'cur': cur_value,
                'delta': delta,
                'pct': pct,
                'verdict': Verdict(delta, pct, max_pct) if delta is not None else '⚪',
            })
    cur_aar = current['aar']['size']
    base_aar = baseline['aar']['size'] if baseline else None
    delta = cur_aar - base_aar if base_aar is not None else None
    rows.append({
        'abi': 'aar (whole)',
        'label': 'File size',
        'base': base_aar,
        'cur': cur_aar,
        'delta': delta,
        'pct': Percent(delta, base_aar) if delta is not None else None,
        'verdict': Verdict(delta, Percent(delta, base_aar), max_pct) if delta is not None else '⚪',
    })
    return rows


def RenderRow(row):
    base = FmtBytes(row['base']) if row['base'] is not None else 'N/A'
    if row['delta'] is None:
        delta, pct = 'N/A', 'N/A'
    else:
        delta = FmtDelta(row['delta'])
        pct = '{:+.2f}%'.format(row['pct']) if row['pct'] is not None else 'N/A'
    return '| {} | {} | {} | {} | {} {} | {} |'.format(
        Esc(row['abi']), Esc(row['label']), base, FmtBytes(row['cur']),
        delta, row['verdict'], pct)


def RenderSymbolTable(title, entries, demangler):
    lines = ['<details open>', '<summary>{}</summary>'.format(title), '',
             '| Δ | Symbol |', '|---:|---|']
    for entry in entries:
        lines.append('| {} | `{}` |'.format(
            FmtDelta(entry['delta']),
            Esc(demangler.get(entry['name'], entry['name']))))
    lines += ['</details>', '']
    return lines


def RenderReport(current, baseline, rows, symbol_diffs, max_pct, top):
    lines = [MARKER, '## 📦 Binary Size Report', '']
    if baseline is None:
        lines.append('> ⚪ No baseline yet (first run or baseline cache '
                     'evicted); only absolute sizes are reported this time.')
        lines.append('')
    lines.append('| Artifact | Metric | Baseline (main) | This PR | Δ | Change |')
    lines.append('|---|---|---:|---:|---:|---:|')
    for row in rows:
        lines.append(RenderRow(row))
    lines.append('')
    if baseline is not None:
        for abi, (grown, shrunk) in symbol_diffs.items():
            if not grown and not shrunk:
                lines.append('<details><summary>Symbol-level diff · {} (no changes)</summary></details>'.format(abi))
                lines.append('')
                continue
            names = [e['name'] for e in grown] + [e['name'] for e in shrunk]
            demangler = Demangle(names)
            if grown:
                lines += RenderSymbolTable(
                    'Symbol-level diff · {} — top {} grown (uncompressed)'.format(abi, len(grown)),
                    grown, demangler)
            if shrunk:
                lines += RenderSymbolTable(
                    'Symbol-level diff · {} — top {} shrunk'.format(abi, len(shrunk)),
                    shrunk, demangler)
        lines.append('---')
        lines.append('Baseline: `main@{}` · threshold ±{:.2f}% · full nm dumps '
                     'are in the run Artifacts'.format(
                         baseline.get('head_sha', '')[:8], max_pct))
    lines.append('')
    return '\n'.join(lines)


def CmdCompare(args):
    current = LoadJson(os.path.join(args.current, 'size-data.json'))
    baseline_path = os.path.join(args.baseline, 'size-data.json')
    baseline = LoadJson(baseline_path) if os.path.isfile(baseline_path) else None
    rows = BuildRows(current, baseline, args.max_pct)

    symbol_diffs = {}
    if baseline is not None:
        for cur in current['artifacts']:
            base = next((item for item in baseline['artifacts']
                         if item['abi'] == cur['abi']), None)
            if base is None:
                continue
            cur_symbols = ParseNm(os.path.join(args.current, cur['nm']))
            base_symbols = ParseNm(os.path.join(args.baseline, base['nm']))
            symbol_diffs[cur['abi']] = SymbolDiff(
                cur_symbols, base_symbols, args.top)

    report = RenderReport(current, baseline, rows, symbol_diffs,
                          args.max_pct, args.top)
    if args.render:
        WriteText(args.render, report)
        print('wrote {}'.format(args.render))

    exceeded = [row for row in rows
                if row['pct'] is not None and abs(row['pct']) > args.max_pct]
    for row in exceeded:
        print('::warning title=BinarySize::{} {} {} ({:+.2f}%) exceeds '
              'threshold {:.2f}%'.format(row['abi'], row['label'],
                                         FmtBytes(row['cur']),
                                         row['pct'], args.max_pct))
    if args.fail and exceeded:
        sys.exit(2)


def RunGit(git_args, check=True):
    result = subprocess.run(['git'] + git_args, capture_output=True, text=True)
    if check and result.returncode != 0:
        Die('git {} failed:\n{}'.format(' '.join(git_args), result.stderr))
    return result


def RenderTrendReadme(history):
    abis = sorted({abi for point in history for abi in point['sizes']})
    dates = [point['date'][5:].replace('-', '/') for point in history]
    lines = ['# skity binary size trend', '',
             '> This branch is updated automatically by CI; do not edit '
             'manually.', '',
             '## Latest snapshot', '',
             '| ABI | Uncompressed | Compressed |', '|---|---:|---:|']
    latest = history[-1]
    for abi in abis:
        entry = latest['sizes'].get(abi)
        if entry:
            lines.append('| {} | {} | {} |'.format(
                abi, FmtBytes(entry['size']), FmtBytes(entry['compressed'])))
    lines.append('')
    lines.append('Latest commit: `{}`'.format(latest['sha']))
    lines.append('')

    def chart_block(title, metric):
        block = ['```mermaid', 'xychart-beta',
                 '    title "{}"'.format(title),
                 '    x-axis [{}]'.format(', '.join('"{}"'.format(d) for d in dates)),
                 '    y-axis "KB"']
        for abi in abis:
            values = []
            last = None
            for point in history:
                entry = point['sizes'].get(abi)
                if entry:
                    last = entry[metric]
                values.append('{:.1f}'.format((last or 0) / 1024.0))
            block.append('    line "{}" [{}]'.format(abi, ', '.join(values)))
        block.append('```')
        return block

    lines.append('## Uncompressed size trend')
    lines.append('')
    lines += chart_block('libskity.so uncompressed size (KB)', 'size')
    lines.append('')
    lines.append('## Compressed size trend')
    lines.append('')
    lines += chart_block('libskity.so compressed size (KB)', 'compressed')
    lines.append('')
    return '\n'.join(lines)


def CmdTrend(args):
    data = LoadJson(args.data)
    fetch = RunGit(['fetch', args.remote, args.branch], check=False)
    if fetch.returncode == 0:
        RunGit(['switch', '-C', args.branch, 'FETCH_HEAD'])
    else:
        # first run: create an orphan branch holding only the generated files
        RunGit(['switch', '--orphan', args.branch])

    history_path = 'size-history.jsonl'
    history = []
    if os.path.isfile(history_path):
        with open(history_path) as handle:
            history = [json.loads(line) for line in handle if line.strip()]
    point = {
        'date': data['created_at'][:10],
        'sha': data.get('head_sha', '')[:10],
        'sizes': {item['abi']: {'size': item['size'],
                                'compressed': item['compressed']}
                  for item in data['artifacts']},
    }
    history.append(point)
    history = history[-args.max_points:]
    with open(history_path, 'w') as handle:
        for entry in history:
            handle.write(json.dumps(entry, sort_keys=True) + '\n')
    WriteText('README.md', RenderTrendReadme(history))

    RunGit(['add', history_path, 'README.md'])
    RunGit(['-c', 'user.name=skity-size-bot', '-c',
            'user.email=actions@github.com', 'commit', '-m',
            'size: update trend ({})'.format(point['sha'])])
    RunGit(['push', args.remote, 'HEAD:refs/heads/{}'.format(args.branch)])
    print('updated {} on {} ({} data points)'.format(args.branch, args.remote,
                                                     len(history)))


def Main():
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest='command', required=True)

    collect = subparsers.add_parser('collect')
    collect.add_argument('--aar-glob', required=True)
    collect.add_argument('--out', required=True)
    collect.add_argument('--head-sha', default='')
    collect.add_argument('--pr-number', default='')
    collect.add_argument('--event', default='')
    collect.set_defaults(func=CmdCollect)

    compare = subparsers.add_parser('compare')
    compare.add_argument('--current', required=True)
    compare.add_argument('--baseline', required=True)
    compare.add_argument('--render', default='')
    compare.add_argument('--top', type=int, default=15)
    compare.add_argument('--max-pct', type=float, default=1.0)
    compare.add_argument('--fail', action='store_true')
    compare.set_defaults(func=CmdCompare)

    trend = subparsers.add_parser('trend')
    trend.add_argument('--data', required=True)
    trend.add_argument('--branch', default='size-data')
    trend.add_argument('--remote', default='origin')
    trend.add_argument('--max-points', type=int, default=60)
    trend.set_defaults(func=CmdTrend)

    args = parser.parse_args()
    args.func(args)


if __name__ == '__main__':
    Main()
