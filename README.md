# Improv Pulse — JUCE 8 Audio & SFZ Grand Piano Blues Sampler

A C++17 / JUCE 8 audio development workspace featuring a real-time **Accurate Salamander Grand Piano V6.2 12-Bar E Minor Blues Sampler (`PianoDemo`)** at 100 BPM, a dedicated **SFZ Instrument Loader & Verification Utility (`sfz_instrument_loader`)**, and a system audio/GUI verification utility (`JuceTest`).

---

## Project Structure

- **`sfz_instrument_loader/`**:
  - A C++17 / JUCE 8 SFZ instrument parser (`SfzParser`), shared sample buffer cache (`SampleBufferCache`), Catmull-Rom 4-point cubic interpolation voice engine (`SfzVoice` & `SfzSynthesiser`), and multi-threaded progressive background loader (`AsyncSfzLoaderThread`).
  - Loads the **Accurate Salamander Grand Piano V6.2 (48kHz / 24-bit Stereo)** soundbank from `sfz_instrument_loader/salamander_piano/` (480 unique 48kHz 24-bit WAV files mapped across 1,408 SFZ regions covering all 88 piano keys A0–C8 across 16 velocity layers `v01`–`v16`).
  - Includes a standalone CLI verification and benchmarking executable (`sfz_instrument_loader`) that parses `.sfz` files, loads all 480 WAV samples in parallel (<0.4s total, <0.06s for primary playability), and verifies polyphonic audio synthesis across dynamic layers (`pp`, `mf`, `ff`).
- **`PianoDemo/`**: A JUCE 8 GUI application integrated with `sfz_instrument_loader` that plays a sample-accurate **12-Bar E Minor Blues progression at 100 BPM** using the Accurate Salamander Grand Piano V6.2 soundbank.
  - Features an interactive 12-bar chord grid with live beat LEDs (click any bar to jump), real-time SFZ Tone Preset selector (`Flat - Recommended`, `Bass +1.0/+1.5 dB`, `Treble +0.5..+2.5 dB`), turnaround & arrangement style selectors, tempo/volume controls, and an 88-key `juce::MidiKeyboardComponent`.
- **`JuceTest/`**: Minimal JUCE 8 verification app testing GUI rendering and a 440 Hz sine wave audio callback.
- **`TUTORIAL.md`**: Detailed C++ developer's guide to Digital Audio Programming, JUCE 8, SFZ sampling, and real-time sequencing in this codebase.

---

## Build & Run Instructions (Linux)

### 1. System Dependencies (Ubuntu / Debian / gLinux)

```bash
sudo apt update && sudo apt install -y \
  build-essential cmake pkg-config \
  libasound2-dev libasound2-plugins alsa-utils \
  libjack-jackd2-dev \
  libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev \
  libgl1-mesa-dev libglu1-mesa-dev \
  libcurl4-openssl-dev \
  libfreetype6-dev libfontconfig1-dev \
  unzip
```

### 2. Download & Extract Accurate Salamander Grand Piano V6.2 Soundbank

Because the 48kHz 24-bit studio WAV recordings are ~1.9 GB, they are excluded from Git via `.gitignore`. Download the archive from the official Accurate-Salamander project page and extract it into `sfz_instrument_loader/salamander_piano/`:

- **Official Website**: [https://www.ir.isas.jaxa.jp/~cyamauch/AccurateSalamander/](https://www.ir.isas.jaxa.jp/~cyamauch/AccurateSalamander/)
- **Archive Name**: `AccurateSalamanderGrandPianoV6.2RC2_48khz24bit.zip`

```bash
mkdir -p sfz_instrument_loader/salamander_piano
unzip -q ~/Downloads/AccurateSalamanderGrandPianoV6.2RC2_48khz24bit.zip \
  -d sfz_instrument_loader/salamander_piano
```

### 3. Build & Run `sfz_instrument_loader` (Standalone SFZ Loader & Verification)

```bash
cd sfz_instrument_loader
cmake -B build -S .
cmake --build build -j$(nproc)
./build/sfz_instrument_loader_artefacts/Release/sfz_instrument_loader
```

Optional flags:
```bash
# Test a specific SFZ preset and render output verification audio to WAV:
./build/sfz_instrument_loader_artefacts/Release/sfz_instrument_loader \
  --sfz salamander_piano/sfz_daw/Accurate-SalamanderGrandPiano_treble1.0db.sfz \
  --render-wav test_output.wav
```

### 4. Build & Run `PianoDemo` (100 BPM E Minor Blues Sampler)

```bash
cd PianoDemo
cmake -B build -S .
cmake --build build -j$(nproc)
./build/PianoDemo_artefacts/Debug/PianoDemo
```

### 5. Build & Run `JuceTest` (440 Hz Audio Verification)

```bash
cd JuceTest
cmake -B build -S .
cmake --build build -j$(nproc)
./build/JuceTest_artefacts/Debug/JuceTest
```

---

## Windows Setup & Build Instructions

### 1. Download & Extract Soundbank (PowerShell)

Download `AccurateSalamanderGrandPianoV6.2RC2_48khz24bit.zip` from [https://www.ir.isas.jaxa.jp/~cyamauch/AccurateSalamander/](https://www.ir.isas.jaxa.jp/~cyamauch/AccurateSalamander/) and extract it:

```powershell
New-Item -ItemType Directory -Force -Path sfz_instrument_loader\salamander_piano
Expand-Archive -Path "$HOME\Downloads\AccurateSalamanderGrandPianoV6.2RC2_48khz24bit.zip" `
               -DestinationPath "sfz_instrument_loader\salamander_piano"
```

### 2. Build & Run `sfz_instrument_loader` on Windows

```powershell
cd sfz_instrument_loader
cmake -B build -S .
cmake --build build --config Release
.\build\sfz_instrument_loader_artefacts\Release\sfz_instrument_loader.exe
```

### 3. Build & Run `PianoDemo` on Windows

```powershell
cd ..\PianoDemo
cmake -B build -S .
cmake --build build --config Release
.\build\PianoDemo_artefacts\Release\PianoDemo.exe
```
