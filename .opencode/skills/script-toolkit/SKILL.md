---
name: script-toolkit
description: General-purpose script toolkit covering text, file, data, network, development, and system utilities. Use when the user asks to process text/files/data, check networks, run dev workflows, or inspect system resources. Use ONLY when one of the individual sub-skills (text-utils, file-utils, data-utils, net-utils, dev-utils, sys-utils, content-utils) does not clearly cover the task.
---

# Script Toolkit

A collection of small focused utilities in `scripts/`. Each tool does one thing well and is designed to be chained with pipes.

## Quick Reference

| Category | Scripts | Use Case |
|---|---|---|
| Text | `summarize.sh`, `extract.sh`, `encode-decode.sh` | File preview, grep, encoding |
| File | `file-organize.sh`, `detect-format.sh`, `note-create.sh` | Organize, identify, create notes |
| Data | `json-pretty.py`, `data-stats.py` | JSON/CSV processing |
| Network | `check-url.sh` | URL health checks |
| Dev | `diff-summary.sh`, `template-gen.sh`, `workflow-run.sh` | Git diffs, templates, automation |
| System | `sys-info.sh` | Resource monitoring |

## Calling Convention

All scripts live in `scripts/` relative to the project root:
```
scripts/summarize.sh somefile.txt
command | scripts/extract.sh pattern
```

## Guidelines
- Scripts accept piped input when a file argument is omitted
- All scripts print usage when called with no arguments
- Prefer the specialized sub-skills first; use this skill as a fallback
