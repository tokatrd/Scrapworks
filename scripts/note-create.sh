#!/bin/bash
# note-create.sh - Create a timestamped note file
# Usage: note-create.sh [title] [--dir <path>]

DIR="${HOME}/notes"
TITLE=""

while [ $# -gt 0 ]; do
  case "$1" in
    --dir) DIR="$2"; shift 2 ;;
    *)     TITLE="$*"; break ;;
  esac
done

mkdir -p "$DIR"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
SLUG=$(echo "${TITLE:-note}" | tr '[:upper:]' '[:lower:]' | sed 's/[^a-z0-9]/-/g' | sed 's/--*/-/g' | sed 's/^-//;s/-$//')
FILENAME="${DIR}/${TIMESTAMP}_${SLUG}.md"

cat > "$FILENAME" <<EOF
# ${TITLE:-Note}

**Date:** $(date '+%Y-%m-%d %H:%M')
**Tags:**

---

EOF

echo "Created: $FILENAME"
