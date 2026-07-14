#!/bin/bash
# detect-format.sh - Detect file type and format info
# Usage: detect-format.sh <file>

FILE="$1"
[ -z "$FILE" ] && echo "Usage: detect-format.sh <file>" >&2 && exit 1
[ ! -f "$FILE" ] && echo "File not found: $FILE" >&2 && exit 1

echo "=== File: $FILE ==="
echo "  Size: $(stat -c%s "$FILE" 2>/dev/null || stat -f%z "$FILE" 2>/dev/null) bytes"
echo "  Type: $(file -b "$FILE")"
echo "  Lines: $(wc -l < "$FILE")"
echo "  Encoding: $(file -b --mime-encoding "$FILE" 2>/dev/null || echo 'unknown')"

ext="${FILE##*.}"
case "$ext" in
  json) echo "  JSON: $(python3 -c "import json; json.load(open('$FILE')); print('valid')" 2>/dev/null || echo 'invalid')" ;;
  csv)  echo "  CSV: $(head -1 "$FILE" | awk -F',' '{print NF}') columns" ;;
  py|js|ts|sh|rb|go|rs) echo "  Shebang: $(head -1 "$FILE")" ;;
esac
