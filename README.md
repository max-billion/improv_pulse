# Improv Pulse — JUCE 8 Audio & Piano Blues Sampler

A C++17 / JUCE 8 audio development workspace featuring a real-time **Steinway Model B 12-Bar E Minor Blues Piano Sampler (`PianoDemo`)** at 100 BPM and a system audio/GUI verification utility (`JuceTest`).

---

## Project Structure

- **`PianoDemo/`**: A JUCE 8 application that loads University of Iowa MIS Steinway Model B grand piano recordings across 3 velocity layers (`pp`, `mf`, `ff`), automatically trims leading silence from raw studio takes, normalizes dynamic layers, and loops a sample-accurate **12-Bar E Minor Blues progression at 100 BPM**.
  - Includes an interactive 12-bar chord grid with live beat LEDs (click any bar to jump), turnaround & arrangement style selectors, tempo/volume controls, and an 88-key `juce::MidiKeyboardComponent`.
- **`JuceTest/`**: Minimal JUCE 8 verification app testing GUI rendering and a 440 Hz sine wave audio callback.
- **`download_and_organize_piano.py`**: Script to download and organize the 88-key Steinway Model B samples (`pp`, `mf`, `ff` in AIFF and WAV formats) and generate `manifest.csv`, `manifest.json`, and SFZ files in `piano_samples/`.
- **`piano_samples/`**: Sample metadata (`manifest.csv`, `manifest.json`), SFZ definitions (`piano.sfz`, `piano_wav.sfz`), and downloaded audio samples.

---

## Windows Setup & Build Instructions

### 1. Install Dependencies (Windows 10 / 11)

On Windows, JUCE uses native operating system audio and graphics APIs (**WASAPI**, **DirectSound**, and **Direct2D/Win32**) automatically—no external audio or GUI development libraries need to be installed.

You only need **Visual Studio 2022** (with C++ support), **CMake**, **Git**, and **Python 3** (to download the audio samples).

You can install all prerequisites via **winget** in **PowerShell (Admin)** or **Windows Terminal**:

```powershell
# 1. Install Git, CMake, and Python 3
winget install --id Git.Git -e --source winget
winget install --id Kitware.CMake -e --source winget
winget install --id Python.Python.3.12 -e --source winget

# 2. Install Visual Studio 2022 Community with the C++ Desktop workload
winget install --id Microsoft.VisualStudio.2022.Community --override "--add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --passive"
```

*(Alternatively, download and install [Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/) and check **"Desktop development with C++"** in the installer).*

---

### 2. Clone Repository & Download Piano Samples (Windows)

Open **PowerShell** or **Command Prompt** (restart your terminal after installing tools so `git`, `cmake`, and `python` are on your `PATH`):

```powershell
git clone https://github.com/max-billion/improv_pulse.git
cd improv_pulse

# Download and organize the 88-key Steinway Model B WAV/AIFF samples (~2.7 GB)
python download_and_organize_piano.py
```

---

### 3. Build & Run `PianoDemo` on Windows (100 BPM E Minor Blues Sampler)

From **PowerShell** or **Command Prompt**:

```powershell
cd PianoDemo

# Generate Visual Studio solution & project files (fetches JUCE 8 automatically)
cmake -B build -S .

# Compile in Release mode (recommended for smooth real-time audio)
cmake --build build --config Release

# Run the compiled executable
.\build\PianoDemo_artefacts\Release\PianoDemo.exe
```

#### Optional: Open in Visual Studio IDE
You can also open the generated Visual Studio solution directly to edit, build, and debug:

```powershell
start build\PianoDemo.sln
```
*(In Visual Studio, right-click **`PianoDemo`** in the Solution Explorer -> **Set as Startup Project**, select **Release** or **Debug** in the top toolbar, and press **F5**).*

---

### 4. Build & Run `JuceTest` on Windows (440 Hz Verification)

```powershell
cd ..\JuceTest
cmake -B build -S .
cmake --build build --config Release
.\build\JuceTest_artefacts\Release\JUCE Installation Test.exe
```

---

## Linux Setup & Build Instructions (Ubuntu / Debian / gLinux)

### 1. System Dependencies (Linux)

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

### 2. Download Piano Samples (Linux)

```bash
python3 download_and_organize_piano.py
```

### 3. Build & Run `PianoDemo` (Linux)

```bash
cd PianoDemo
cmake -B build -S .
cmake --build build -j$(nproc)
./build/PianoDemo_artefacts/Debug/PianoDemo
```

### 4. Build & Run `JuceTest` (Linux)

```bash
cd JuceTest
cmake -B build -S .
cmake --build build -j$(nproc)
./build/JuceTest_artefacts/Debug/JuceTest
```
