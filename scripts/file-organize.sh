#!/bin/bash
# file-organize.sh - Organize files in a directory by extension
# Usage: file-organize.sh [directory] [--dry-run]

DIR="${1:-.}"
DRY_RUN=false
[ "$2" = "--dry-run" ] && DRY_RUN=true

find "$DIR" -maxdepth 1 -type f | while read -r file; do
  [ ! -f "$file" ] && continue
  ext="${file##*.}"
  [ "$ext" = "$file" ] && ext="no_ext"
  target="$DIR/$ext"
  if [ "$DRY_RUN" = true ]; then
    echo "[DRY-RUN] mkdir -p '$target' && mv '$file' '$target/'"
  else
    mkdir -p "$target"
    mv "$file" "$target/"
    echo "Moved: $file -> $target/"
  fi
done

echo "Done."
