---
name: sys-utils
description: System monitoring and resource information utilities. Use when the user asks about CPU, memory, disk usage, system info, or running processes.
---

# System Utilities

Scripts for quick system introspection.

## Available Tools

### sys-info.sh
Quick system resource overview with selective flags.
```
scripts/sys-info.sh           # everything
scripts/sys-info.sh --cpu     # CPU cores and model
scripts/sys-info.sh --mem     # memory usage
scripts/sys-info.sh --disk    # disk usage
```

## Guidelines
- Use `--cpu`, `--mem`, or `--disk` for targeted queries instead of parsing `/proc` directly
- The `--all` default shows top CPU-consuming processes
