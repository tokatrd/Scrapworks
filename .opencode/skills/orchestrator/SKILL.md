---
name: orchestrator
description: Orchestrator that dispatches to the right sub-skill based on user intent. Use when the user's request could be handled by one of text-utils, file-utils, data-utils, net-utils, dev-utils, sys-utils, or content-utils skills. Map the request to the correct skill and delegate via the task tool with the appropriate subagent type.
---

# Orchestrator

This skill does NOT execute tools directly. It routes the user's request to the correct specialized sub-skill by identifying the domain and dispatching a task call.

## Dispatch Rules

| If the user asks about… | Dispatch to skill-agent with skill: |
|---|---|
| Summarizing, extracting, searching, encoding/decoding text | text-utils |
| Organizing files, detecting formats | file-utils |
| Pretty-printing JSON, CSV stats, data exploration | data-utils |
| Checking if a URL is reachable, response time | net-utils |
| Git diffs, templates, running multi-step workflows | dev-utils |
| CPU, memory, disk usage, system processes | sys-utils |
| Creating notes, documents, written content | content-utils |

## Dispatch Method

Use the `task` tool to delegate:
```
task skill-agent: "Run text-utils: summarize the file README.md"
task skill-agent: "Run data-utils: validate and pretty-print config.json"
```

When multiple parallel tasks are possible, dispatch concurrently to different skill agents (skill-agent, skill-agent-2, etc.) to maximize throughput.

## Ambiguous Requests

If the request spans multiple domains:
1. Split into sub-tasks
2. Dispatch each sub-task in parallel to separate skill agents
3. Merge the results

If the domain is unclear, fall back to the script-toolkit skill for broad coverage.
