---
name: nowrite
description: ensure agents dont change files when background work is ongoing
allowed-tools: bash, read_file, sed, grep, find
---

This skill simply states that no changes should be made to the file system during the operation of the agent's task.
Agent is free to read any files. Human is working on the code at the same time and may be changing files or running tests.
Agen may write docs and plans in ai-output/.