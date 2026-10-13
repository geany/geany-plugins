# geanyprogress — AI Agent Plan Progress Panel

## Status

| Phase | Description                              | Status  |
|-------|------------------------------------------|---------|
| 1     | Plugin scaffold & build system           | complete |
| 2     | Unix domain socket server                | complete |
| 3     | JSON parsing + in-memory model           | complete |
| 4     | GTK panel UI                             | complete |
| 5     | Persistence (.planning/state/)           | complete |
| 6     | geanyenv integration + helper script     | complete |

## Notes

- Plan document: `ai-context/geanyprogress-plan.md`
- Architecture: Unix domain socket at `/tmp/geany-progress-<PID>.sock`, env var `GEANY_PROGRESS_SOCK`
- To resume after `/clear`:
  "Continue geanyprogress Phase N per ai-context/geanyprogress-plan.md. Current progress is in ai-context/PROGRESS.md."

---

# geanyvosk — Implementation Progress

## Status

| Phase | Description                          | Status   |
|-------|--------------------------------------|----------|
| 1     | Project scaffold & build system      | complete |
| 2     | ALSA microphone capture thread       | complete |
| 3     | Vosk ASR + wake word detection       | complete |
| 4     | UI command dispatch via geanycontrol | complete |
| 5     | Skill & AI-prompt creation by voice  | complete |
| 6     | Agent skill execution by voice       | complete |

## Notes

- Plan document: `ai-context/geanyvosk-plan.md`
- Vosk is NOT installed on the system; must be installed before Phase 3.
  - Install libvosk: download from https://alphacephei.com/vosk/models (C library + header)
  - Install ALSA dev: `sudo apt install libasound2-dev`
- To resume after `/clear`, start a new session and say:
  "Continue geanyvosk Phase N per ai-context/geanyvosk-plan.md"
