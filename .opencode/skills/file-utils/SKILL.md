---
name: file-utils
description: File organization, detection, and note creation utilities. Use when the user asks to organize, sort, rename files, detect file types, or create notes.
---

# File Utilities

Small scripts for managing files and directories.

## Available Tools

### file-organize.sh
Group files in a directory by extension into subfolders.
```
scripts/file-organize.sh <directory>
scripts/file-organize.sh <directory> --dry-run   # preview only
```

### detect-format.sh
Detect file type, encoding, size, and validation status.
```
scripts/detect-format.sh <file>
```

### note-create.sh
Create a timestamped markdown note file.
```
scripts/note-create.sh "Meeting notes"
scripts/note-create.sh "My Title" --dir ~/docs
```

## Guidelines
- Always use `--dry-run` with `file-organize.sh` first to preview
- `detect-format.sh` validates JSON/CSV and reports column count
- Notes are created in `~/notes/` by default
