# Improv Pulse — JUCE 8 Audio & Piano Blues Sampler

A C++17 / JUCE 8 audio development workspace featuring a real-time **Steinway Model B 12-Bar E Minor Blues Piano Sampler (`PianoDemo`)** at 100 BPM and a system audio/GUI verification utility (`JuceTest`).

---

## Project Structure

- **`PianoDemo/`**: A JUCE 8 application that loads University of Iowa MIS Steinway Model B grand piano recordings across 3 velocity layers (`pp`, `mf`, `ff`), automatically trims leading silence from raw studio takes, normalizes dynamic layers, and loops a sample-accurate **12-Bar E Minor Blues progression at 100 BPM**.
  - Includes an interactive 12-bar chord grid with live beat LEDs (click any bar to jump), turnaround & arrangement style selectors, tempo/volume controls, and an 88-key `juce::MidiKeyboardComponent`.
- **`JuceTest/`**: Minimal JUCE 8 verification app testing GUI rendering and a 440 Hz sine wave audio callback via ALSA/JACK.
- **`download_and_organize_piano.py`**: Script to download and organize the 88-key Steinway Model B samples (`pp`, `mf`, `ff` in AIFF and WAV formats) and generate `manifest.csv`, `manifest.json`, and SFZ files in `piano_samples/`.
- **`piano_samples/`**: Sample metadata (`manifest.csv`, `manifest.json`), SFZ definitions (`piano.sfz`, `piano_wav.sfz`), and downloaded audio samples.

---

## 1. System Dependencies (Ubuntu / Debian / gLinux)

```bash
sudo apt update && sudo apt install -y \
  build-essential cmake pkg-config \
  libasound2-dev libasound2-plugins alsa-utils \
  libjack-jackd2-dev \
  libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev \
  libgl1-mesa-dev libglu1-mesa-dev \
  libcurl4-openssl-dev \
  libfreetype6-dev libfontconfig1-dev
```

---

## 2. Download Piano Samples

If `piano_samples/wav/` is not yet populated on your machine, run:

```bash
python3 download_and_organize_piano.py
```

---

## 3. Build & Run `PianoDemo` (100 BPM E Minor Blues Sampler)

```bash
cd PianoDemo
cmake -B build -S .
cmake --build build -j$(nproc)
./build/PianoDemo_artefacts/Debug/PianoDemo
```

---

## 4. Build & Run `JuceTest` (440 Hz Verification)

```bash
cd JuceTest
cmake -B build -S .
cmake --build build -j$(nproc)
./build/JuceTest_artefacts/Debug/JuceTest
```
