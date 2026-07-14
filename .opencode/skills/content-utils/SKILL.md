---
name: content-utils
description: Content creation and note-taking utilities. Use when the user asks to create, format, or organize notes, documents, or written content.
---

# Content Utilities

Scripts for creating and managing written content.

## Available Tools

### note-create.sh
Create a timestamped markdown note with front-matter.
```
scripts/note-create.sh "Project ideas"
scripts/note-create.sh "API design thoughts" --dir ~/Documents
```

### template-gen.sh
Generate documents from templates with variable substitution.
```
scripts/template-gen.sh docs/templates/report.md TITLE="Q3 Report" AUTHOR="User"
```

## Guidelines
- Notes get `YYYYMMDD_HHMMSS_slug.md` filenames
- Templates use `{{VAR}}` syntax and can create any text format
