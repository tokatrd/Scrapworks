---
name: text-utils
description: Text summarization, extraction, and encoding utilities. Use when the user asks to summarize, extract, grep, search, encode, or decode text.
---

# Text Utilities

A set of small focused tools for text processing. Run them via `scripts/<tool>` from the project root.

## Available Tools

### summarize.sh
Summarize file contents or piped input. Shows first N lines with line numbers.
```
scripts/summarize.sh <file>
command | scripts/summarize.sh
scripts/summarize.sh <file> --lines 20
```

### extract.sh
Extract lines matching a pattern (grep wrapper).
```
scripts/extract.sh <pattern> <file>
scripts/extract.sh -r <regex> <file>
command | scripts/extract.sh <pattern>
```

### encode-decode.sh
Base64, hex, and URL encode/decode.
```
scripts/encode-decode.sh b64enc <data>
scripts/encode-decode.sh b64dec <encoded>
scripts/encode-decode.sh urlenc "hello world"
echo "data" | scripts/encode-decode.sh b64enc
```

## Guidelines
- Prefer `summarize.sh` over raw `head` for file previews
- Use `extract.sh` instead of raw `grep` when you need line numbers
- Forward piped data whenever possible
