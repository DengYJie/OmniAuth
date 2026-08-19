# AGENTS.md

## Project Overview

OmniAuth is a Qt 6 desktop authentication system for coursework around "intelligent login". The baseline assignment scope is:

- username/password login with local user storage
- face login with local model inference
- AI-assisted captcha / behavior risk verification

The repository already goes beyond the minimum assignment scope. Keep those enhancements unless the user explicitly asks to remove them. Existing enhancements include:

- SMS code login
- password recovery flow
- face enrollment management after login
- anti-spoofing before face matching
- local/remote repository split for auth, captcha, face, and risk services
- FluentQt-based desktop UI instead of a plain Qt Widgets form

## Architecture

The codebase follows a layered structure:

- `src/ui`: Qt Widgets pages, view models, navigation, window chrome, and custom widgets
- `src/domain`: entities, DTOs, repositories, use cases, and behavior tracking models
- `src/data/local`: SQLite access, local ONNX/OpenCV inference engines, and local data sources
- `src/data/remote`: remote data source stubs/adapters
- `src/data/repository`: repository implementations that switch between local and remote backends
- `src/data/di`: dependency wiring via `AppContainer`
- `src/core`: crypto utilities
- `res/models`: ONNX models copied next to the executable after build
- `scripts`: Python utilities for behavior-risk data generation and model training

Primary entry points:

- App startup: `src/main.cpp`
- Dependency graph: `src/data/di/AppContainer.cpp`
- Auth shell window: `src/ui/screen/main/AuthWindow.cpp`
- Main logged-in window: `src/ui/screen/main/MainWindow.cpp`

## Tech Stack

- C++23
- Qt 6 (`Core`, `Gui`, `Widgets`, `Sql`)
- CMake
- vcpkg for `libsodium`
- OpenCV for face/image processing
- ONNX Runtime for local inference
- FluentQt and QWindowKit for desktop UI
- SQLite for local persistence
- Python scripts for behavior-model training and sample generation

## Core Functional Notes

- Password hashing is implemented with `libsodium` (`crypto_pwhash_str`), not raw SHA-256. Do not downgrade this unless the user explicitly requires assignment-only parity.
- User records are stored in SQLite under the application data directory, not the repository root at runtime.
- Phone data and face feature blobs are encrypted before storage.
- Face login uses `retinaface.onnx`, `minifasnet.onnx`, and `arcface.onnx`.
- Behavior risk inference uses `res/models/behavior_mlp.onnx`.
- `AppContainer::init(false)` in `src/main.cpp` means the app currently starts in local-backend mode.

## Prerequisites

Recommended Windows setup for this repository:

- Qt 6.x with `QTDIR` pointing at the Qt installation prefix
- Visual Studio 2019 toolchain or compatible MSVC toolchain
- CMake 3.16+
- Ninja
- vcpkg with `VCPKG_ROOT` set
- OpenCV discoverable by CMake

Do not assume these tools are available on `PATH`. On this project, agents should first verify the actual local install locations or use absolute executable paths when needed.

This repository uses `FetchContent` to download:

- `qwindowkit`
- ONNX Runtime binaries

Network access may be required during the first configure/build.

## Setup Commands

PowerShell examples from the repository root. These are examples only; if `cmake`, `python`, `ninja`, or Qt tooling are not on `PATH`, use the machine's absolute executable paths instead.

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_PREFIX_PATH="$env:QTDIR"
```

```powershell
cmake --build build
```

If you need a Release build:

```powershell
cmake -S . -B build-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_PREFIX_PATH="$env:QTDIR"
cmake --build build-release
```

## Run Workflow

The executable is produced in the selected build directory. Post-build steps copy:

- `res/models/*` into `<build>/.../models`
- ONNX Runtime DLL/shared library next to the executable
- OpenCV runtime binaries on Windows when detected

Run the built app from the build output directory so model-relative paths resolve correctly.

## Python Model Scripts

Behavior-risk utilities live in `scripts/`.

Generate bot samples:

```powershell
python scripts/generate_bot_samples.py
```

Train and export the behavior model:

```powershell
python scripts/train_behavior_mlp.py
```

The training script expects:

- `data/bot.csv`
- `data/human.csv`

It writes:

- `res/models/behavior_mlp.onnx`
- `res/models/scaler.pkl`

Note: `scaler.pkl` is a training artifact; C++ inference uses the exported ONNX model directly.

If Python is not on `PATH`, invoke the script with the full interpreter path.

## Data and Models

- Local SQLite schema initialization is in `src/data/local/LocalUserDataSource.cpp`.
- Runtime DB file is created under `QStandardPaths::AppDataLocation` as `omniauth_core.db`.
- A repository-root `omniauth_core.db` may exist for local inspection, but runtime logic uses the app data path.
- Face feature vectors are stored as encrypted float blobs in `sys_users.face_encodings`.
- Model filenames under `res/models` are part of the runtime contract. Do not rename them without updating the loader paths.

## Development Rules

- Preserve the layered split: UI -> use case/service -> repository -> local/remote data source.
- Prefer adding behavior in view models and use cases rather than putting business logic into widget classes.
- Keep expensive inference or I/O off the UI thread.
- When changing face or risk inference, verify both model loading paths and post-build asset copying.
- Do not remove existing enhancements just to match the minimum homework wording.
- Treat remote data sources as extension points; many are intentionally light or placeholder implementations.

## Testing and Verification

There is currently no formal automated test suite in this repository.

For code changes, verify with the smallest relevant checks:

- successful CMake configure
- successful build
- manual run of the desktop app
- manual validation of affected login flow
- manual validation that required models are present in the output directory

When touching behavior-model scripts, also verify:

- sample generation completes
- ONNX export succeeds
- updated model is copied into the app output directory

## UI and Coursework Expectations

This is a coursework project, but the current code already targets a more polished desktop experience:

- custom window chrome
- Fluent-style components
- multi-page auth flow
- inline captcha and face-scan UX

Maintain that quality level when editing UI. Do not replace the existing FluentQt-based structure with a minimal form unless explicitly requested.

## Security Notes

- Do not commit secrets, real biometric data, or private production credentials.
- Do not replace password hashing with plaintext, MD5, or unsalted SHA-256.
- Preserve encryption for stored phone numbers and face features.
- Be careful when changing `QSettings`-stored crypto material logic in `LocalUserDataSource`.

## Known Gaps

- The assignment text mentions SHA-256, but the implementation uses a stronger libsodium password-hashing API.
- Some remote data source classes appear to be scaffolding rather than full integrations.
- Automated tests and CI are not present.

## Agent Workflow Tips

- Before editing, check `git status` because the worktree may already contain user changes.
- Prefer `rg --files` and `rg` for navigation and search.
- Use `AppContainer` and the existing repository interfaces as the source of truth for how features are wired.
- If adding new homework features, integrate them into the current architecture instead of creating parallel one-off code paths.
- Before running build or script commands, confirm whether the required executables are exposed via `PATH`; if not, switch to absolute paths instead of rewriting project config around a local machine quirk.
