# Verification — 2026-10-07

## Executed in Linux

- C++17 core build with GCC 13.3.
- Deterministic pitch tests: 44.1 / 48 / 96 kHz, approximately C2, A2, C4, A4, A5; <12-cent error.
- Longest stable note selection in a recording containing two pitches.
- Silence and deterministic broadband noise rejection.
- Loop bounds, finite output, and seam discontinuity checks.
- ADSR attack/sustain/release timing and filter impulse stability.
- Actual shared VoiceBank: three-note chord energy, sustain pedal, release completion, pitch bend, 32-voice cap, panic.
- Processor.cpp and Editor.cpp syntax checked against JUCE 8.0.6 headers, with -Wall -Wextra -Wpedantic and no warnings.
- AddressSanitizer and UndefinedBehaviorSanitizer passed on the core tests. Leak checking was disabled because the container cannot expose the required process metadata.
- Shell scripts passed bash syntax checking.

The build environment used for automated local checks is Linux. On 2026-10-07 the user separately built the Universal targets on their MacBook and supplied a screenshot confirming standalone startup. AU scanning and performance remain unverified. The original photorealistic design image is a concept, not an application screenshot.

## Required before a release

- [x] Universal build succeeds on the user's MacBook; standalone startup confirmed by screenshot.
- [ ] Optional Intel runtime test on an Intel Mac (cross-compilation alone does not test this).
- [ ] auval succeeds, Logic scans and opens the AU.
- [ ] Standalone microphone permission, input-device selection and unmuted input recording work.
- [ ] Audio reaches AU recording input through Logic's sidechain routing.
- [ ] 44.1/48/96 kHz sessions, device switches and project reopen.
- [ ] Held chord → pedal down → note off → pedal up; no stuck notes.
- [ ] Native input recording, WAV/AIFF import, silent/short/unsupported recordings.
- [ ] Save/load audio and parameters; save/open Logic project including tape.
- [ ] Boundary dragging, very short loops, repeated note-ons and 32-voice stealing.
- [ ] Listen for clicks at loops, aggressive automation and rapid preset changes.
- [ ] Readability and hit targets at 1100×740 on a MacBook display.
- [ ] CPU use during 32 voices and long-sample analysis; no audio-thread stalls.
- [ ] Developer ID signing/notarization and distribution notices before public binary release.
