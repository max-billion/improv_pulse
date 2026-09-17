# University of Iowa MIS - Steinway Model B Piano Samples

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
