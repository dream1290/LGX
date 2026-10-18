# Working rules for agents in this repository

## Authority
- Work on branches and open pull requests. Never push to `main`.
- The human owner performs: merges, repository setting changes, secrets,
  tags and releases. Do not attempt them.

## Evidence
1. Never write a performance number in a commit message, README, or repo
   description unless it links to benchmark output committed in the same
   change. Numbers live in `docs/05-performance/` and
   `benchmarks/`; commit messages reference those files, they do not
   restate the numbers.
2. Before saying "done": clone the repository fresh into a temp directory
   and build and test from that clone. A build tree that already exists
   proves nothing about what a new contributor gets.
3. Run `git status` and `git diff --stat` before every commit. Never
   commit binaries, logs, editor/agent config, probe output, or generated
   CSVs.
4. A commit message states what changed and how it was verified. It does
   not state what was achieved ("faster", "robust", "complete") without
   the reproducing command.

## Writing
5. User-facing docs (README, descriptions, changelogs): no emojis, no
   checkmark tables, no "robust", "comprehensive", "seamless",
   "guaranteed". Internal plan files under `docs/superpowers/plans/` may
   use status markers.
6. If a claim in a doc cannot be verified from this repository, delete it
   or label it unverified.

## Gates
7. Never weaken or bypass a gate to make something pass. A rejected push,
   failing ratchet, or red check is information; report it.
