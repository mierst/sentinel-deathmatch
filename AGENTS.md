# Guidance for AI-assisted contributors

Read [CONTRIBUTING.md](./CONTRIBUTING.md) for Sentinel Deathmatch's engineering and testing requirements, and [RELEASING.md](./RELEASING.md) before release work. If this checkout also contains CLAUDE.md, read its shared project guidance. Keep project guidance in its source documents rather than duplicating it here. Resolve references relative to this file in the active checkout or worktree.

When reading instructions written for another assistant, retain project facts, conventions, and operational constraints. Do not adopt that assistant's identity, model choices, or tool and runtime configuration; use the capabilities and instruction precedence of your current environment.

## Optional delegation

If you use subagents, assign bounded tasks with clear ownership and avoid conflicting parallel edits. The coordinating contributor remains responsible for design, integration, and verification against the project's requirements. Delegation is optional and must respect the contributor's permissions and resource limits.
