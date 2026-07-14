#!/bin/bash
# sys-info.sh - Quick system resource overview
# Usage: sys-info.sh [--all|--cpu|--mem|--disk]

MODE="${1:---all}"

case "$MODE" in
  --cpu)
    echo "=== CPU ==="
    nproc | xargs -I{} echo "  Cores: {}"
    grep "model name" /proc/cpuinfo | head -1 | sed 's/.*: //' | xargs -I{} echo "  Model: {}"
    ;;
  --mem)
    echo "=== Memory ==="
    free -h | awk '/Mem:/ {print "  Total: " $2 "  Used: " $3 "  Free: " $4}'
    ;;
  --disk)
    echo "=== Disk ==="
    df -h / | awk 'NR==2 {print "  Total: " $2 "  Used: " $3 "  Avail: " $4 "  Use: " $5}'
    ;;
  --all|*)
    echo "=== System ==="
    echo "  Host: $(hostname)"
    echo "  Kernel: $(uname -r)"
    echo "  Uptime: $(uptime -p | sed 's/up //')"
    echo "  Load:  $(uptime | awk -F'load average:' '{print $2}' | xargs)"
    echo ""
    bash "$0" --cpu
    echo ""
    bash "$0" --mem
    echo ""
    bash "$0" --disk
    echo ""
    echo "=== Top Processes (by CPU) ==="
    ps aux --sort=-%cpu | head -4 | awk '{printf "  %-20s %5s%%\n", $11, $3}'
    ;;
esac
