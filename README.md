# Enigma

![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat&logo=cplusplus&logoColor=white)
![GCC](https://img.shields.io/badge/GCC-15.2-A42E2B?style=flat&logo=gnu&logoColor=white)
![MSYS2](https://img.shields.io/badge/MSYS2-MINGW64-374151?style=flat)

This project is an enigma.

## 📑 Table of Contents

1. [✨ Features](#-features)
2. [🧠 How It Works](#-how-it-works)
3. [🚀 Usage](#-usage)
4. [🛠️ Building from Source](#️-building-from-source)
5. [🧪 Testing](#-testing)
6. [🗂️ Project Structure](#️-project-structure)
7. [📄 License](#-license)

## ✨ Features

- Classified. 


## 🧠 How It Works

- Classified.


## 🚀 Usage

```
enigma.exe [--seed <n>] [--help]
```

| Option | Effect |
|---|---|
| `--seed <n>` | Seeds the generator with `<n>`, an integer from 0 to 18446744073709551615. The same seed reproduces the same table. |
| `-h`, `--help` | Prints the usage text and exits. |

Without `--seed`, every run prints a different table. The seed of such a run is not shown, so to reproduce a table, choose a seed and pass it.

**Example:** `build\enigma.exe --seed 42`

```
Program:     Enigma
Version:     1.0
Incarnation: 15

-------------------------------------------------------------------
i     y      x_0    x_1    x_2    x_3    x_4    x_5    x_6    x_7
-------------------------------------------------------------------
0000 -0.882 -0.219 -0.975  0.047  0.371  0.275  0.653  0.891  0.506
0001 -0.959 -0.102 -0.906 -0.871  0.495 -0.701 -0.149 -0.800 -0.712
0002 -0.986 -0.812  0.060 -0.108  0.546 -0.755  0.490 -0.352  0.502
0003 -0.356  0.609 -0.913  0.852  0.487 -0.168 -0.405 -0.961  0.400
0004 -0.000 -0.081  0.074 -0.022 -0.577  0.214 -0.014 -0.232  0.334
0005 -0.082 -0.168 -0.131 -0.817 -0.852  0.641 -0.045 -0.113  0.991
0006 -0.856  0.190  0.238  0.154  0.723 -0.049  0.648 -0.016 -0.111
0007  0.140 -0.380 -0.406  0.046 -0.270  0.528 -0.389  0.372  0.458
0008  0.656  0.301  0.633  0.574  0.304  0.588 -0.195  0.873 -0.304
0009 -0.755 -0.401  0.476  0.188 -0.295 -0.723  0.249  0.185  0.043
0010 -0.606  0.226 -0.882 -0.266  0.471  0.224 -0.209  0.865  0.481
0011 -0.816 -0.540  0.058 -0.752  0.575 -0.531 -0.815  0.780 -0.056
0012 -0.825 -0.774  0.385 -0.746  0.440  0.210 -0.288  0.538  0.858
0013 -0.114  0.993 -0.219 -0.257  0.665  0.530  0.317  0.767 -0.869
0014  0.029 -0.351 -0.414 -0.167 -0.679  0.703 -0.028 -0.418  0.638
0015 -0.760 -0.143 -0.840  0.752  0.584  0.807  0.710 -0.947  0.489
-------------------------------------------------------------------
min  -0.986 -0.812 -0.975 -0.871 -0.852 -0.755 -0.815 -0.961 -0.869
max   0.656  0.993  0.633  0.852  0.723  0.807  0.710  0.891  0.991
avg  -0.448 -0.103 -0.235 -0.087  0.187  0.112  0.033  0.089  0.228
sum  -7.172 -1.652 -3.762 -1.393  2.988  1.792  0.530  1.431  3.649
-------------------------------------------------------------------
```

The `Incarnation:` line shows the build number of the executable you run. Every other line is the same on every build.


## 🛠️ Building from Source

### Prerequisites

- Windows 11 on x86-64.
- [MSYS2](https://www.msys2.org/) with the MINGW64 GCC toolchain, version 15.2 or later (`pacman -S mingw-w64-x86_64-gcc`), and `C:\msys64\mingw64\bin` on `PATH`.

The build needs neither Make nor CMake: `build.bat` runs `g++` directly.

### Steps

1. Clone the repository:

    ```
    git clone https://github.com/rohingosling/enigma.git
    cd enigma
    ```

2. Build it:

    ```
    build.bat
    ```

3. Run it:

    ```
    build\enigma.exe --seed 42
    ```

In PowerShell, write `.\build.bat`. The script changes to its own directory first, so it works from any current directory.

| Command | Effect |
|---|---|
| `build.bat` | Builds `build\enigma.exe` and, on success, increments the incarnation counter |
| `build.bat test` | Builds and runs `build\enigma_test.exe`, and returns its exit code |
| `build.bat clean` | Deletes both executables |

The release build compiles with:

```
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off -static -s -DENIGMA_INCARNATION=<n>
```

`-Werror` turns every warning into a build failure. `-static` makes the executable self-contained. To confirm it, run `objdump -p build\enigma.exe | findstr /c:"DLL Name"`, which should list only Windows system DLLs. Do not add `-ffast-math`, `-Ofast`, or `-march=native`: each can change floating-point results, and seeded runs would no longer match the reference reports.

### Incarnation Counter

`incarnation.txt` holds the number of the last successful release build. `build.bat` compiles with the next number and writes it back only if compiling and linking succeed, so every number belongs to an executable that was actually built. `build.bat test` and `build.bat clean` leave the counter alone. A build that calls `g++` directly, without `-DENIGMA_INCARNATION`, reports `Incarnation: 0`.

## 🧪 Testing

```
build.bat test
```

The test build compiles every module in `src\` except `main.cpp`, together with `test\test_main.cpp`, using the release flags without `-s` and the incarnation. It runs the result and returns its exit code. It prints only failed checks, each as `file:line:` followed by the check, and then a summary line:

```
252 checks, 0 failed
```

The checks cover the engine's known-answer test, the uniform mapping and its bounds, `tanh` against `std::tanh` across [−20, 20], the draw order, the column statistics, number formatting, and the report's layout rules over seeds 0 to 999. The files in `test\golden\` are reviewed reference reports for seeds 42 and 6, rendered with incarnation 1. The full report must match them character for character. A mismatch on another toolchain means that seeds do not reproduce there.

## 🗂️ Project Structure

```
enigma
├─ build.bat                    Build driver: release build, test build, clean
├─ incarnation.txt              Build counter, bumped by each successful release build
├─ LICENSE                      MIT License
├─ README.md                    This file
├─ src                          Application source
│  ├─ main.cpp                  Entry point; runs one pipeline and prints the report
│  ├─ build_info.h              Program name, version, and incarnation
│  ├─ command_line.h / .cpp     Parses --seed and --help
│  ├─ random_source.h / .cpp    Seeded mt19937_64 mapped to [-1, 1)
│  ├─ perceptron.h / .cpp       8-input neuron and its tanh transfer function
│  ├─ table.h / .cpp            Draws each row's inputs and evaluates y
│  ├─ statistics.h / .cpp       Per-column min, max, avg, and sum
│  └─ report.h / .cpp           Renders the report as text
│
└─ test                         Test harness
   ├─ test_main.cpp             Unit and golden-output test runner
   └─ golden                    Reviewed reference reports
      ├─ report_seed_42.txt     Standard 16-row report
      └─ report_seed_6.txt      Report whose columns widen to 7 characters
```

## 📄 License

Released under the [MIT License](LICENSE). Copyright © 2026 Rohin Gosling.
