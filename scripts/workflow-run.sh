#!/bin/bash
# workflow-run.sh - Run a sequence of commands with logging
# Usage: workflow-run.sh <steps-file>
# Steps file format (one step per line):
#   name | command
# Example:
#   Lint | npm run lint
#   Test | npm test
#   Build | npm run build

STEPS_FILE="$1"
[ -z "$STEPS_FILE" ] && echo "Usage: workflow-run.sh <steps-file>" >&2 && exit 1
[ ! -f "$STEPS_FILE" ] && echo "Steps file not found: $STEPS_FILE" >&2 && exit 1

LOG_FILE="workflow-$(date +%Y%m%d_%H%M%S).log"
TOTAL=0
PASSED=0
FAILED=0

echo "=== Workflow: $(date) ===" | tee -a "$LOG_FILE"

while IFS='|' read -r name cmd; do
  name="${name// /}"
  cmd="${cmd// /}"
  [ -z "$name" ] && continue
  TOTAL=$((TOTAL + 1))
  printf "  [%d/%d] %s ... " "$TOTAL" "$(wc -l < "$STEPS_FILE")" "$name" | tee -a "$LOG_FILE"
  if eval "$cmd" >> "$LOG_FILE" 2>&1; then
    echo "PASS" | tee -a "$LOG_FILE"
    PASSED=$((PASSED + 1))
  else
    echo "FAIL" | tee -a "$LOG_FILE"
    FAILED=$((FAILED + 1))
  fi
done < "$STEPS_FILE"

echo "=== Result: $PASSED/$TOTAL passed, $FAILED failed ===" | tee -a "$LOG_FILE"
exit $FAILED
