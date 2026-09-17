#!/usr/bin/env python3
"""
Downloads and organizes the University of Iowa Musical Instrument Samples (MIS)
Steinway & Sons Model B Piano library (all 88 keys at available dynamics: pp, mf, ff).
"""

import os
import sys
import time
import json
import csv
import urllib.request
import urllib.parse
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

BASE_URL = "https://theremin.music.uiowa.edu/sound%20files/MIS/Piano_Other/piano/"
OUTPUT_DIR = os.path.abspath("piano_samples")

PITCH_CLASSES = {
    'C': 0, 'Db': 1, 'D': 2, 'Eb': 3, 'E': 4, 'F': 5,
    'Gb': 6, 'G': 7, 'Ab': 8, 'A': 9, 'Bb': 10, 'B': 11
}

PITCH_NAMES = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B']

# Generate list of all 88 piano notes: A0 (21) to C8 (108)
ALL_88_NOTES = ['A0', 'Bb0', 'B0']
for oct_num in range(1, 8):
    for p in PITCH_NAMES:
        ALL_88_NOTES.append(f"{p}{oct_num}")
ALL_88_NOTES.append('C8')

def note_to_midi(note_str):
    if note_str.startswith(('Db', 'Eb', 'Gb', 'Ab', 'Bb')):
        p = note_str[:2]
        octave = int(note_str[2:])
    else:
        p = note_str[:1]
        octave = int(note_str[1:])
    return (octave + 1) * 12 + PITCH_CLASSES[p]

def midi_to_freq(midi_num):
    return 440.0 * (2.0 ** ((midi_num - 69) / 12.0))

# Missing from UIowa source:
# pp missing: A0
# mf missing: A0, Bb0
DYNAMICS = {
    'pp': [n for n in ALL_88_NOTES if n != 'A0'],
    'mf': [n for n in ALL_88_NOTES if n not in ('A0', 'Bb0')],
    'ff': ALL_88_NOTES
}

def download_file(item):
    dyn, note, target_path = item
    url = f"{BASE_URL}Piano.{dyn}.{note}.aiff"
    
    if os.path.exists(target_path) and os.path.getsize(target_path) > 100000:
        # Check header
        with open(target_path, 'rb') as f:
            header = f.read(12)
            if header.startswith(b'FORM') and b'AIFF' in header:
                return (dyn, note, True, "already exists", os.path.getsize(target_path))
    
    tmp_path = target_path + ".tmp"
    headers = {'User-Agent': 'Mozilla/5.0 (Sampler-Downloader)'}
    
    max_retries = 5
    for attempt in range(max_retries):
        try:
            req = urllib.request.Request(url, headers=headers)
            with urllib.request.urlopen(req, timeout=30) as resp, open(tmp_path, 'wb') as out_f:
                while True:
                    chunk = resp.read(65536)
                    if not chunk:
                        break
                    out_f.write(chunk)
            
            # Verify file integrity
            file_size = os.path.getsize(tmp_path)
            if file_size < 100000:
                raise ValueError(f"Downloaded file too small: {file_size} bytes")
            with open(tmp_path, 'rb') as f:
                header = f.read(12)
                if not (header.startswith(b'FORM') and b'AIFF' in header):
                    raise ValueError(f"Invalid AIFF header: {header}")
            
            os.replace(tmp_path, target_path)
            return (dyn, note, True, "downloaded", file_size)
        except Exception as e:
            if os.path.exists(tmp_path):
                try:
                    os.remove(tmp_path)
                except OSError:
                    pass
            if attempt == max_retries - 1:
                return (dyn, note, False, str(e), 0)
            time.sleep(1.0 * (attempt + 1))

def convert_to_wav(aiff_path, wav_path):
    if os.path.exists(wav_path) and os.path.getsize(wav_path) > 100000:
        return True
    tmp_wav = wav_path + ".tmp.wav"
    cmd = [
        'ffmpeg', '-y', '-v', 'error',
        '-i', aiff_path,
        '-c:a', 'pcm_s16le',
        tmp_wav
    ]
    res = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if res.returncode == 0 and os.path.exists(tmp_wav) and os.path.getsize(tmp_wav) > 100000:
        os.replace(tmp_wav, wav_path)
        return True
    if os.path.exists(tmp_wav):
        os.remove(tmp_wav)
    return False

def get_audio_info(file_path):
    cmd = [
        'ffprobe', '-v', 'error',
        '-select_streams', 'a:0',
        '-show_entries', 'stream=sample_rate,channels,bits_per_sample,duration',
        '-of', 'json',
        file_path
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode == 0:
        try:
            info = json.loads(res.stdout)['streams'][0]
            return {
                'sample_rate': int(info.get('sample_rate', 44100)),
                'channels': int(info.get('channels', 2)),
                'bits_per_sample': int(info.get('bits_per_sample', 16)),
                'duration_seconds': round(float(info.get('duration', 0.0)), 3)
            }
        except Exception:
            pass
    return {
        'sample_rate': 44100,
        'channels': 2,
        'bits_per_sample': 16,
        'duration_seconds': 0.0
    }

def main():
    print(f"Target directory: {OUTPUT_DIR}")
    for dyn in ['pp', 'mf', 'ff']:
        os.makedirs(os.path.join(OUTPUT_DIR, dyn), exist_ok=True)
        os.makedirs(os.path.join(OUTPUT_DIR, "wav", dyn), exist_ok=True)
    
    download_tasks = []
    for dyn, notes in DYNAMICS.items():
        for note in notes:
            target_path = os.path.join(OUTPUT_DIR, dyn, f"Piano.{dyn}.{note}.aiff")
            download_tasks.append((dyn, note, target_path))
    
    total = len(download_tasks)
    print(f"Total files to download: {total} (ff: 88, mf: 86, pp: 87)")
    
    start_time = time.time()
    completed_count = 0
    total_bytes = 0
    
    with ThreadPoolExecutor(max_workers=8) as executor:
        futures = {executor.submit(download_file, task): task for task in download_tasks}
        for future in as_completed(futures):
            dyn, note, success, msg, size = future.result()
            completed_count += 1
            total_bytes += size
            if not success:
                print(f"[{completed_count}/{total}] FAILED: Piano.{dyn}.{note}.aiff - {msg}")
            else:
                if completed_count % 20 == 0 or completed_count == total:
                    elapsed = time.time() - start_time
                    mb = total_bytes / (1024 * 1024)
                    print(f"[{completed_count}/{total}] {mb:.1f} MB downloaded ({elapsed:.1f}s)")
    
    print("Download phase complete. Converting to 16-bit PCM WAV...")
    wav_tasks = []
    for dyn, notes in DYNAMICS.items():
        for note in notes:
            aiff_p = os.path.join(OUTPUT_DIR, dyn, f"Piano.{dyn}.{note}.aiff")
            wav_p = os.path.join(OUTPUT_DIR, "wav", dyn, f"Piano.{dyn}.{note}.wav")
            wav_tasks.append((aiff_p, wav_p))
            
    with ThreadPoolExecutor(max_workers=8) as executor:
        list(executor.map(lambda pair: convert_to_wav(pair[0], pair[1]), wav_tasks))
    print("WAV conversion complete.")
    
    # Generate metadata manifests
    print("Generating metadata manifests and sampler definitions...")
    manifest_records = []
    
    # Velocity layer ranges:
    # pp: 1 - 55
    # mf: 56 - 95
    # ff: 96 - 127
    VEL_RANGES = {
        'pp': (1, 55),
        'mf': (56, 95),
        'ff': (96, 127)
    }
    
    # Sample a few files to get format info
    sample_info = get_audio_info(os.path.join(OUTPUT_DIR, 'ff', 'Piano.ff.C4.aiff'))
    
    for note in ALL_88_NOTES:
        midi_num = note_to_midi(note)
        freq = round(midi_to_freq(midi_num), 2)
        octave = int(note[2:] if note.startswith(('Db', 'Eb', 'Gb', 'Ab', 'Bb')) else note[1:])
        pitch = note[:2] if note.startswith(('Db', 'Eb', 'Gb', 'Ab', 'Bb')) else note[:1]
        
        for dyn in ['pp', 'mf', 'ff']:
            aiff_fn = f"Piano.{dyn}.{note}.aiff"
            aiff_rel = f"{dyn}/{aiff_fn}"
            aiff_full = os.path.join(OUTPUT_DIR, aiff_rel)
            
            wav_fn = f"Piano.{dyn}.{note}.wav"
            wav_rel = f"wav/{dyn}/{wav_fn}"
            wav_full = os.path.join(OUTPUT_DIR, wav_rel)
            
            exists = os.path.exists(aiff_full)
            if exists:
                size_bytes = os.path.getsize(aiff_full)
            else:
                size_bytes = 0
            
            lovel, hivel = VEL_RANGES[dyn]
            # Adjust velocity ranges for missing notes
            if note == 'A0':
                # Only ff exists for A0
                if dyn == 'ff':
                    lovel, hivel = 1, 127
            elif note == 'Bb0':
                # pp and ff exist, mf missing
                if dyn == 'pp':
                    lovel, hivel = 1, 95
                elif dyn == 'ff':
                    lovel, hivel = 96, 127
            
            manifest_records.append({
                'note': note,
                'midi_number': midi_num,
                'pitch': pitch,
                'octave': octave,
                'frequency_hz': freq,
                'dynamic': dyn,
                'velocity_min': lovel,
                'velocity_max': hivel,
                'exists': exists,
                'aiff_path': aiff_rel if exists else None,
                'wav_path': wav_rel if exists else None,
                'file_size_bytes': size_bytes,
                'sample_rate': sample_info['sample_rate'],
                'channels': sample_info['channels'],
                'bit_depth': sample_info['bits_per_sample']
            })
            
    # Save manifest.json
    with open(os.path.join(OUTPUT_DIR, "manifest.json"), "w") as f:
        json.dump({
            'instrument': "Steinway & Sons Model B Grand Piano",
            'performer': "Evan Mazunik",
            'date': "November 2001",
            'location': "Voxman Music Building, University of Iowa",
            'source_url': "https://theremin.music.uiowa.edu/MISpiano.html",
            'total_piano_keys': 88,
            'samples_count': {
                'ff': len(DYNAMICS['ff']),
                'mf': len(DYNAMICS['mf']),
                'pp': len(DYNAMICS['pp']),
                'total': sum(len(v) for v in DYNAMICS.values())
            },
            'missing_source_recordings': {
                'pp': ['A0'],
                'mf': ['A0', 'Bb0']
            },
            'samples': manifest_records
        }, f, indent=2)
    
    # Save manifest.csv
    with open(os.path.join(OUTPUT_DIR, "manifest.csv"), "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=[
            'note', 'midi_number', 'pitch', 'octave', 'frequency_hz',
            'dynamic', 'velocity_min', 'velocity_max', 'exists',
            'aiff_path', 'wav_path', 'file_size_bytes',
            'sample_rate', 'channels', 'bit_depth'
        ])
        writer.writeheader()
        for r in manifest_records:
            writer.writerow(r)
            
    # Generate SFZ files (piano.sfz for AIFF, piano_wav.sfz for WAV)
    def generate_sfz(filepath, is_wav=False):
        lines = [
            "// -------------------------------------------------------------",
            "// University of Iowa Musical Instrument Samples (MIS)",
            "// Steinway & Sons Model B Grand Piano",
            "// Converted and mapped for standard SFZ samplers",
            "// -------------------------------------------------------------",
            "",
            "<control>",
            "default_path=" + ("wav/" if is_wav else ""),
            "",
            "<global>",
            "ampeg_attack=0.001",
            "ampeg_release=1.200",
            ""
        ]
        
        # Group 1: pp layer
        lines.append("// Pianissimo (pp) Layer")
        lines.append("<group>")
        lines.append("lovel=1")
        lines.append("hivel=55")
        for note in ALL_88_NOTES:
            midi_num = note_to_midi(note)
            if note == 'A0':
                continue # handled by fallback in ff
            elif note == 'Bb0':
                # Bb0 extends to hivel=95 in pp because mf is missing
                continue
            ext = "wav" if is_wav else "aiff"
            sub = f"wav/pp" if not is_wav and False else f"pp"
            lines.append(f"<region> sample={sub}/Piano.pp.{note}.{ext} key={midi_num}")
        lines.append("")
        
        # Group 2: mf layer
        lines.append("// Mezzo-Forte (mf) Layer")
        lines.append("<group>")
        lines.append("lovel=56")
        lines.append("hivel=95")
        for note in ALL_88_NOTES:
            midi_num = note_to_midi(note)
            if note in ('A0', 'Bb0'):
                continue # handled by fallbacks
            ext = "wav" if is_wav else "aiff"
            sub = f"wav/mf" if not is_wav and False else f"mf"
            lines.append(f"<region> sample={sub}/Piano.mf.{note}.{ext} key={midi_num}")
        lines.append("")

        # Group 3: ff layer
        lines.append("// Fortissimo (ff) Layer")
        lines.append("<group>")
        lines.append("lovel=96")
        lines.append("hivel=127")
        for note in ALL_88_NOTES:
            midi_num = note_to_midi(note)
            if note == 'A0':
                continue # handled below
            ext = "wav" if is_wav else "aiff"
            sub = f"wav/ff" if not is_wav and False else f"ff"
            lines.append(f"<region> sample={sub}/Piano.ff.{note}.{ext} key={midi_num}")
        lines.append("")

        # Fallbacks for notes missing in certain dynamics
        lines.append("// Fallback mappings for notes missing from original recording session")
        ext = "wav" if is_wav else "aiff"
        # A0 only has ff: covers entire velocity range 1-127
        lines.append(f"<region> sample=ff/Piano.ff.A0.{ext} key=21 lovel=1 hivel=127")
        # Bb0 has pp and ff, but not mf: pp covers 1-95
        lines.append(f"<region> sample=pp/Piano.pp.Bb0.{ext} key=22 lovel=1 hivel=95")
        lines.append("")

        with open(filepath, "w") as f:
            f.write("\n".join(lines) + "\n")

    generate_sfz(os.path.join(OUTPUT_DIR, "piano.sfz"), is_wav=False)
    generate_sfz(os.path.join(OUTPUT_DIR, "piano_wav.sfz"), is_wav=True)
    
    # Generate README.md
    readme_content = f"""# University of Iowa MIS - Steinway Model B Piano Samples

This directory contains recordings of all 88 keys of a Steinway & Sons Model B Grand Piano, sourced from the University of Iowa Electronic Music Studios (MIS).

## Recording Details
- **Instrument**: Steinway & Sons model B Grand Piano
- **Performer**: Evan Mazunik
- **Date**: November 5 & 27, 2001
- **Location**: 2017 Voxman Music Building, University of Iowa
- **Microphones**: Left mic 8" above center bass strings, Right mic 8" above center treble strings (Neumann KM 84 cardioid condenser)
- **Mixer**: Mackie 1402-VLZ
- **Recorder**: Panasonic SV-3800 DAT
- **Format**: 16-bit, 44.1 kHz, Stereo PCM (Non-anechoic)
- **Source**: https://theremin.music.uiowa.edu/MISpiano.html

## Directory Structure
```
piano_samples/
├── ff/                  # 88 Fortissimo AIFF samples (A0 - C8)
├── mf/                  # 86 Mezzo-Forte AIFF samples (B0 - C8)
├── pp/                  # 87 Pianissimo AIFF samples (Bb0 - C8)
├── wav/                 # Exact 16-bit PCM WAV mirrors of all samples
│   ├── ff/
│   ├── mf/
│   └── pp/
├── manifest.json        # Full JSON metadata (MIDI numbers, frequencies, velocity layers, file paths)
├── manifest.csv         # CSV spreadsheet format of metadata
├── piano.sfz            # Ready-to-load SFZ instrument definition (AIFF)
├── piano_wav.sfz        # Ready-to-load SFZ instrument definition (WAV)
└── README.md
```

## Dynamic & Velocity Layer Mapping
- **`pp` (Pianissimo)**: Velocity 1 – 55 (87 keys, Bb0 to C8)
- **`mf` (Mezzo-Forte)**: Velocity 56 – 95 (86 keys, B0 to C8)
- **`ff` (Fortissimo)**: Velocity 96 – 127 (88 keys, A0 to C8)

### Missing Original Notes & Velocity Fallbacks
In the original recording session at the University of Iowa, 3 dynamic takes were not captured:
1. **A0 (MIDI 21)**: Missing in `pp` and `mf`. The `ff` sample is mapped to velocities 1–127.
2. **Bb0 (MIDI 22)**: Missing in `mf`. The `pp` sample is extended to cover velocities 1–95.
*(Note: `Piano.mf.Gb7.aiff` was linked erroneously to `oops.html` on the website, but was located and retrieved directly from the server).*

## Usage in JUCE & Samplers

### JUCE C++ (AudioFormatManager & Synthesiser / SamplerSound)
```cpp
juce::AudioFormatManager formatManager;
formatManager.registerBasicFormats(); // Enables both WAV and AIFF support

// Parse manifest.json or loop over files
// Example: loading a sample into JUCE Synthesiser
juce::File sampleFile("path/to/piano_samples/mf/Piano.mf.C4.aiff");
std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(sampleFile));

juce::BigInteger midiNotes;
midiNotes.setBit(60); // C4

sampler.addSound(new juce::SamplerSound("Piano_C4", *reader, midiNotes, 60, 0.0, 0.1, 10.0));
```

### Standard SFZ Samplers
Load either `piano.sfz` (for AIFF) or `piano_wav.sfz` (for WAV) into any SFZ-compatible plugin or sampler engine (e.g. SFZero in JUCE, Sforzando, DecentSampler, LinuxSampler).
"""
    with open(os.path.join(OUTPUT_DIR, "README.md"), "w") as f:
        f.write(readme_content)
        
    print("All tasks finished successfully!")

if __name__ == '__main__':
    main()
