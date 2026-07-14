---
name: dev-utils
description: Development workflow utilities including diff summary, template generation, and multi-step workflow runner. Use when the user asks to review git history, generate files from templates, or run multi-step processes.
---

# Development Utilities

Scripts to accelerate common development tasks.

## Available Tools

### diff-summary.sh
Compact git diff statistics (file list and line counts).
```
scripts/diff-summary.sh                           # staged vs HEAD
scripts/diff-summary.sh <commit-hash>             # single commit
scripts/diff-summary.sh <commit-a> <commit-b>     # range
```

### template-gen.sh
Substitute {{VAR}} placeholders in template files.
```
scripts/template-gen.sh template.txt NAME=World COUNT=3
```

### workflow-run.sh
Run a sequence of named steps from a steps file with logging.
```
# steps.txt:
# Lint | npm run lint
# Test | npm test
scripts/workflow-run.sh steps.txt
```

## Guidelines
- `diff-summary.sh` is faster than a full `git diff` for quick overviews
- `template-gen.sh` only supports `{{VAR}}` syntax, not `$VAR`
- `workflow-run.sh` logs to a timestamped file and exits with failure count
