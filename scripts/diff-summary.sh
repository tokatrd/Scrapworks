#!/bin/bash
# diff-summary.sh - Summarize a git diff in compact form
# Usage: diff-summary.sh              # staged vs HEAD
#        diff-summary.sh <commit>     # commit vs parent
#        diff-summary.sh <a> <b>      # arbitrary diff

if [ $# -eq 0 ]; then
  git diff --cached --stat
  echo "---"
  git diff --cached --numstat | awk '{added+=$1; deleted+=$2} END {printf "Total: +%d -%d lines\n", added, deleted}'
elif [ $# -eq 1 ]; then
  git diff "$1^" "$1" --stat
elif [ $# -ge 2 ]; then
  git diff "$1" "$2" --stat
fi | head -30
