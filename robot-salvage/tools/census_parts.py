#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Audit native debris factory call sites and the player's format4 part assets.

This produces evidence, not an inferred blanket eligibility list. The runtime
adopts only actual factory results whose SOURCE is a curated mechanical enemy.
No model, texture, disc bytes or absolute user path is written to the report.
"""
import argparse
import ast
import hashlib
import json
import re
import struct
from pathlib import Path


def clean(text):
    pattern = r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    return re.sub(pattern, lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]), text, flags=re.S)


def split_args(text):
    result, start, depth = [], 0, 0
    for i, ch in enumerate(text):
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        elif ch == ',' and depth == 0:
            result.append(text[start:i].strip())
            start = i + 1
    result.append(text[start:].strip())
    return result


def integer(text):
    try:
        value = ast.parse(re.sub(r'(?<=\d)[uUlL]+\b', '', text.strip()), mode='eval').body
        def evaluate(node):
            if isinstance(node, ast.Constant) and type(node.value) is int:
                return node.value
            if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
                return -evaluate(node.operand)
            if isinstance(node, ast.BinOp) and isinstance(node.op, (ast.Add, ast.Sub)):
                a, b = evaluate(node.left), evaluate(node.right)
                return a + b if isinstance(node.op, ast.Add) else a - b
            raise ValueError()
        return evaluate(value)
    except (ValueError, SyntaxError):
        return None


def functions(text):
    # Declarations are skipped. Strings/comments have already been blanked, so
    # braces in them cannot corrupt the definition's balanced range.
    pattern = re.compile(r'^\s*(?!(?:extern|typedef|if|while|switch|for)\b)[\w \t*]+?\b(\w+)\s*\([^;{}]*\)\s*\{', re.M)
    result = []
    for match in pattern.finditer(text):
        start, depth, end = match.end() - 1, 1, match.end()
        while end < len(text) and depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        result.append((start, end, match[1]))
    return result


def callsites(root, label):
    result = []
    for path in sorted(root.rglob('*.c')):
        original = path.read_text(encoding='utf-8', errors='replace')
        text = clean(original)
        aliases = {'SpawnBreakFragment', 'SpawnBreakFragment_r', 'spawn_moby_break_fragments', 'spawn_spinning_moby_fragment', 'FUN_L00_00263fd8'}
        for match in re.finditer(r'\b(\w+)\s*\([^;{}]*?\)\s*__asm__\("(?:func_L00_00265050|FUN_L00_00263fd8)"\)', original, re.S):
            aliases.add(match[1])
        pattern = re.compile(r'\b(' + '|'.join(map(re.escape, sorted(aliases))) + r')\s*\(')
        spans = functions(text)
        for match in pattern.finditer(text):
            context = next((name for start, end, name in spans if start < match.start() < end), None)
            if not context:
                continue
            depth, end = 1, match.end()
            while end < len(text) and depth:
                depth += (text[end] == '(') - (text[end] == ')')
                end += 1
            args = split_args(text[match.end():end-1])
            classes = []
            if match[1] == 'spawn_moby_break_fragments' and len(args) == 7:
                first, count, base, span = [integer(x) for x in args[1:5]]
                if first is not None and count is not None and 0 <= count <= 256:
                    classes.extend(range(first, first+count))
                if base is not None and span is not None and 0 <= span <= 256:
                    classes.extend(range(base, base+span))
            elif match[1] == 'spawn_spinning_moby_fragment':
                classes = list(range(1085, 1089))
            elif len(args) >= 2 and integer(args[1]) is not None:
                classes = [integer(args[1])]
            result.append({'source': label+'/'+path.relative_to(root).as_posix(), 'line': text.count('\n',0,match.start())+1,
                           'function': context, 'factory': match[1], 'fragment_classes': sorted(set(c for c in classes if c > 0)),
                           'literal_class_resolved': bool(classes)})
    return result


def asset_info(levels):
    entries = {}
    for manifest in sorted(levels.glob('level_*/manifest.json')):
        data = json.loads(manifest.read_text(encoding='utf-8-sig'))
        if data.get('format') != 4 or data.get('version') != 'pal':
            raise ValueError('A PAL format4 extraction is required')
        for key, item in data.get('classes', {}).get('moby', {}).items():
            if not isinstance(item.get('mesh'), str):
                entries.setdefault(int(key), []).append({'level': data['level'], 'name': item.get('name',''), 'mesh': None, 'renderable': False})
                continue
            path = (manifest.parent/item['mesh']).resolve()
            if not path.is_relative_to(manifest.parent.resolve()):
                raise ValueError('Unsafe manifest asset path')
            raw = path.read_bytes()
            if len(raw) < 20 or raw[:4] != b'glTF':
                raise ValueError('Invalid GLB: '+path.name)
            json_size = struct.unpack_from('<I', raw, 12)[0]
            gltf = json.loads(raw[20:20+json_size])
            bounds = []
            for mesh in gltf.get('meshes', []):
                for primitive in mesh.get('primitives', []):
                    a = gltf['accessors'][primitive['attributes']['POSITION']]
                    if 'min' in a and 'max' in a:
                        bounds.append([a['min'], a['max']])
            entries.setdefault(int(key), []).append({'level': data['level'], 'name': item.get('name',''),
                'mesh': path.relative_to(levels.resolve()).as_posix(), 'sha256': hashlib.sha256(raw).hexdigest(),
                'native_scale': item.get('scale',1), 'primitive_bounds': bounds,
                'triangles': sum(gltf['accessors'][p['indices']]['count']//3 for m in gltf.get('meshes',[]) for p in m['primitives'] if 'indices' in p)})
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pal-source', type=Path, required=True)
    parser.add_argument('--ntsc-source', type=Path)
    parser.add_argument('--levels', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    calls = callsites(args.pal_source.resolve(), 'pal')
    if args.ntsc_source:
        calls += callsites(args.ntsc_source.resolve(), 'ntsc')
    assets = asset_info(args.levels.resolve())
    builtin = (Path(__file__).resolve().parents[1]/'src/builtin_catalog.cpp').read_text(encoding='utf-8')
    sources = [{'class': int(m[1]), 'stable_key': m[2], 'name_it': m[3], 'name_en': m[4], 'assets': assets.get(int(m[1]), [])}
               for m in re.finditer(r'\{(\d+),"([^"\n]+)","([^"\n]+)","([^"\n]+)"\}', builtin)]
    fragment_classes = sorted({c for call in calls for c in call['fragment_classes']})
    report = {'format': 1, 'kind': 'robot-salvage-source-and-owned-asset-census', 'region': 'pal',
              'inference_is_runtime_eligibility': False,
              'scope': 'Call sites include props and organic sources; only an actual factory result from a curated mechanical source is adopted.',
              'native_gameplay_verified': False, 'source_call_count': len(calls), 'fragment_class_count': len(fragment_classes),
              'mechanical_sources': sources, 'calls': calls,
              'fragment_assets': {str(c): assets.get(c, []) for c in fragment_classes}}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'calls': len(calls), 'fragment_classes': len(fragment_classes), 'mechanical_sources': len(sources),
                      'report_sha256': hashlib.sha256(args.output.read_bytes()).hexdigest()}))


if __name__ == '__main__':
    main()
