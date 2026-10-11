#!/usr/bin/env python3
"""Prepare downloaded recordings as short 48 kHz mono game samples.

Requires ffmpeg on PATH. No synthesis or runtime processing is involved.
Usage: python3 tools/audio/import_recordings.py SOURCE_DIRECTORY
See assets/audio/recipe.json for download URLs, archive members and edits.
"""
import argparse
import array
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import wave
import zipfile


def import_bank(source_dir, destination):
    recipe = json.loads((destination / 'recipe.json').read_text())
    decoded = {}
    source_hashes = {}
    for key, source in recipe['sources'].items():
        path = source_dir / source['local']
        if 'member' in source:
            with zipfile.ZipFile(path) as archive:
                raw = archive.read(source['member'])
            path = source_dir / (key + Path(source['member']).suffix)
            path.write_bytes(raw)
        source_hashes[key] = hashlib.sha256(path.read_bytes()).hexdigest()
        raw = subprocess.check_output([
            'ffmpeg', '-v', 'error', '-i', str(path), '-f', 'f32le',
            '-ac', '1', '-ar', '48000', '-'])
        pcm = array.array('f', raw)
        if sys.byteorder != 'little':
            pcm.byteswap()
        decoded[key] = pcm

    entries = []
    for edit in recipe['samples']:
        source = decoded[edit['source']]
        start = int(edit.get('start', 0) * 48000)
        count = int(edit.get('duration', (len(source) - start) / 48000) * 48000)
        samples = list(source[start:start + count])
        if len(samples) < 2:
            raise ValueError('Empty excerpt: ' + edit['file'])
        if edit.get('trim', True):
            threshold = max(abs(x) for x in samples) * 0.12
            onset = next(i for i, x in enumerate(samples) if abs(x) >= threshold)
            samples = samples[max(0, onset - 48):]
        # Remove DC and optionally darken a physical foley recording with EQ.
        average = sum(samples) / len(samples)
        samples = [x - average for x in samples]
        if 'lowpass' in edit:
            import math
            coefficient = 1 - math.exp(-2 * math.pi * edit['lowpass'] / 48000)
            previous = 0.0
            for i, value in enumerate(samples):
                previous += coefficient * (value - previous)
                samples[i] = previous
        if edit.get('loop', False):
            fade = min(4800, len(samples) // 4)
            for i in range(fade):
                weight = i / fade
                samples[i] = weight * samples[i] + (1 - weight) * samples[-fade + i]
            samples = samples[:-fade]
            samples[-1] = samples[0]
        else:
            attack = min(24, len(samples) // 2)
            tail = min(1440, len(samples) // 4)
            for i in range(attack):
                samples[i] *= i / attack
            for i in range(tail):
                samples[-1 - i] *= i / tail
        peak = max(abs(x) for x in samples)
        if peak < 1e-6:
            raise ValueError('Silent excerpt: ' + edit['file'])
        gain = edit.get('peak', 0.8) / peak
        integers = array.array('h', [round(max(-1, min(1, x * gain)) * 32767) for x in samples])
        if sys.byteorder != 'little':
            integers.byteswap()
        path = destination / edit['file']
        with wave.open(str(path), 'wb') as output:
            output.setnchannels(1)
            output.setsampwidth(2)
            output.setframerate(48000)
            output.writeframes(integers.tobytes())
        entries.append({
            'bank': edit['bank'], 'strength': edit.get('strength', 0),
            'file': edit['file'], 'source': edit['source'],
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
        })
    manifest = {'version': 1, 'sample_rate': 48000, 'samples': entries,
                'source_sha256': source_hashes}
    (destination / 'bank.json').write_text(json.dumps(manifest, indent=2) + '\n')
    size = sum((destination / entry['file']).stat().st_size for entry in entries)
    print(f'Imported {len(entries)} samples ({size / 1048576:.2f} MiB)')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source_directory', type=Path)
    parser.add_argument('--destination', type=Path,
                        default=Path(__file__).resolve().parents[2] / 'assets/audio')
    args = parser.parse_args()
    import_bank(args.source_directory, args.destination)
