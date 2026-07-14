---
name: agent-harnesses
description: Reference for all available agent harnesses (skill agents, MCP servers, and script tools). Use when the user wants to know what capabilities are available or which agent/MCP to use for a task.
---

# Agent Harness Reference

All available execution harnesses and their capabilities.

---

## Skill Agents (MiniCPM5 1B via LM Studio)

| Agent | Mode | Description |
|---|---|---|
| `skill-agent` | subagent | General-purpose. Handles 1 skill call at a time. |
| `skill-agent-2` | subagent | Parallel skill call agent (concurrent with -1) |
| `skill-agent-3` | subagent | Parallel skill call agent (concurrent with -1/-2) |
| `skill-agent-4` | subagent | Parallel skill call agent (concurrent with -1/-2/-3) |

All four agents use `openai/skill-agent` model (MiniCPM5 1B Agentic v8) via LM Studio at localhost:1234, with 8 parallel inference slots total. Dispatch concurrent tasks to different agents for parallel execution.

---

## MCP Server Harnesses (deployed on 192.168.0.2)

| MCP | URL | Tools Exposed |
|---|---|---|
| **duckduckgo-mcp** | :8020 | Privacy web search, content scraping. No API key needed. |
| **docker-mcp** | :8021 | list/run/stop/remove containers, pull/build/push/remove images, manage volumes/networks |
| **fetch-mcp** | :8022 | `fetch` — fetch any URL, return as markdown or HTML |
| **filesystem-mcp** | :8024 (SSE) | read/write/edit/search files within `/home`, directory tree, file info |
| **sqlite-mcp** | :8025 | `read_query`, `write_query`, `list_tables`, `describe_table`, `create_table` |
| **ntfy-mcp** | :8023 | `send_notification`, `fetch_notifications` — push alerts via ntfy |
| **netdata** | :19999 | Real-time CPU, memory, disk, network, and process metrics |
| **mcphub** | :8090/mcp | Aggregator — if the direct MCP URL fails, try through MCPHub |

All MCPs are deployed as Docker containers behind Nginx Proxy Manager.

---

## Script Toolkit (local scripts/)

| Script | Category | What It Does |
|---|---|---|
| `scripts/summarize.sh` | Text | Preview first N lines of a file |
| `scripts/extract.sh` | Text | Grep/extract lines matching pattern |
| `scripts/encode-decode.sh` | Text | Base64, hex, URL encode/decode |
| `scripts/json-pretty.py` | Data | Pretty-print, validate, minify, extract keys from JSON |
| `scripts/data-stats.py` | Data | Column stats on CSV, key stats on JSON |
| `scripts/file-organize.sh` | File | Group files by extension into folders |
| `scripts/detect-format.sh` | File | Detect type, encoding, validation of files |
| `scripts/note-create.sh` | Content | Create timestamped markdown notes |
| `scripts/check-url.sh` | Network | HTTP health check with timing |
| `scripts/diff-summary.sh` | Dev | Compact git diff statistics |
| `scripts/template-gen.sh` | Dev | Substitute `{{VAR}}` in template files |
| `scripts/workflow-run.sh` | Dev | Run multi-step process with logging |
| `scripts/sys-info.sh` | System | CPU, memory, disk, process overview |

---

## Dispatch Flow

```
User Request
     │
     ▼
Orchestrator skill ──→ identifies domain ──→ dispatches via task tool
     │                                              │
     ├─ text/file/data/net/dev/sys/content          │
     │  → sub-script via bash tool ─────────────────┘
     │
     └─ MCP-accessible task
        → calls MCP tool directly
```

For MCP tasks, the agent can invoke tools from any connected MCP server directly.
For script tasks, the orchestrator dispatches a sub-agent task calling the appropriate script.
