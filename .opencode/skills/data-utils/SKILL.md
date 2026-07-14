---
name: data-utils
description: JSON and CSV data processing utilities. Use when the user asks to pretty-print, validate, extract keys, or compute statistics on JSON or CSV data.
---

# Data Utilities

Scripts for structured data formats.

## Available Tools

### json-pretty.py
Pretty-print, validate, minify, or extract nested keys from JSON.
```
scripts/json-pretty.py < file.json
scripts/json-pretty.py file.json --key data.users
command | scripts/json-pretty.py --minify
```

### data-stats.py
Compute statistics on CSV columns or JSON structure.
```
scripts/data-stats.py data.csv
scripts/data-stats.py data.csv --col Status   # value counts
scripts/data-stats.py data.json
```

## Guidelines
- Always pipe or redirect, never type JSON inline as an argument
- Use `--key` with dot-separated paths to drill into nested objects
- `data-stats.py` automatically detects CSV vs JSON by extension
