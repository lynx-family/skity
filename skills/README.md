# Skills Directory

This directory contains project-specific agent skills.

## Structure

Each skill should live in its own subdirectory under `skills/`, for example:

- `skills/<skill-name>/SKILL.md`

`SKILL.md` is the source of truth for that skill's purpose, usage, options, and workflow.

## Adding a New Skill

1. Create a directory under `skills/`.
2. Add a `SKILL.md` with complete usage instructions.
3. Ensure the skill integrates with existing project tooling.

## Maintenance

- Keep skill details in the corresponding `SKILL.md`.
- Avoid duplicating detailed skill docs in top-level repo documents.

## Command Syntax Convention

- Conversation trigger syntax (for agents) may use `$skill-name ...`.
- Shell usage must use real executable commands (for example `python3 tools/code_format_check.py ...`).
- Do not present `$skill-name ...` as a directly runnable terminal command.
