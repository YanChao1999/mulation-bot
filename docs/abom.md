# abom in this repo

This repository dogfoods [abom](https://github.com/YanChao1999/abom) as an agent-material supply chain, and also ships a first-party Cursor skill for mulation itself.

| File | Role |
|------|------|
| `abom.lock.json` | Pins installed recipe commits + material digests |
| `.abom/project.json` | Nearest-workspace wiring prefs (Cursor) |
| `.cursor/skills/mulation/` · `skills/mulation/` | First-party skill: wire mutation gates, filters, discovery |

## Local

```bash
pip install -U abom
abom sync
abom use abom-skill
abom lock --check
abom advisories
abom report abom-skill
```

For mulation workflows in Cursor, load the project skill
[`.cursor/skills/mulation/SKILL.md`](../.cursor/skills/mulation/SKILL.md)
(or the copy under `skills/mulation/`).

## CI

`.github/workflows/abom.yml` installs `abom==0.0.4`, runs `sync` + `lock --check`, then `check` / `report` for every locked tool. The job fails on check failure or advisory hits.
