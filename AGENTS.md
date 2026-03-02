# Project Agent Guide

This file is a lightweight index for agent-related workflows in this repo.
If not stated otherwise, follow the rules in this file as defaults.
Rule priority: Repository Boundaries > Validation Workflow > Coding Style > other references.

## Build and Dependency Docs

Build and dependency instructions: `docs/BUILD_AND_RUN.md`.

## Local Environment Overrides (Optional)

- Optionally use `AGENTS.local.md` to describe machine-specific local development context.
- If `AGENTS.local.md` exists, read it for local environment details before build/test work.
- Typical local details include build directory location, current CMake config options, toolchain paths, host platform info, and where `test-runner` should be executed.
- `AGENTS.local.md` is intended for local-only overrides and should not change shared repository defaults in this file.

## Skills Directory

Skills are under `skills/`; structure guidance is in `skills/README.md`; skill behavior is defined in each `SKILL.md`.

## Coding Style (Brief)

- Follow a C++ style close to Google C++ Style Guide.
- For public headers (for example, files under `include/`), prefer `#include <...>` when possible.
- Use PascalCase for class member function names (camel case with uppercase first letter), for example `ComputeBounds()`.
- Keep naming and formatting consistent with existing code in the touched module.

## Repository Boundaries

- Do not directly edit files under `third_party/`. If a third-party change is required, apply it by adding or updating patch files under `patches/`.
- Avoid modifying public headers (for example, files under `include/`) unless absolutely necessary for the task.
- Any newly added source file must include the repository-standard license/header comment.
- The copyright year in new file headers is fixed and must start from `2021`.
- Use the following exact header text for newly added files:
  `// Copyright 2021 The Lynx Authors. All rights reserved.`
  `// Licensed under the Apache License Version 2.0 that can be found in the`
  `// LICENSE file in the root directory of this source tree.`

## Validation Workflow

- If code or tests are added/changed, prefer using the `test-runner` skill to validate relevant test cases before finishing.
- Validate the smallest relevant test set first; expand only if needed.
- If `test-runner` is unavailable in the current session, run the repository standard test commands documented in `docs/BUILD_AND_RUN.md`.

## Done Criteria

- Changes follow Repository Boundaries (including third-party patch flow, public header limits, and required new-file header comment).
- Minimum relevant validation has been run (`test-runner` when available, otherwise `docs/BUILD_AND_RUN.md` commands).
- Final response reports what was validated and the result.
