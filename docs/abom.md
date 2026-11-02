# abom in this repo

This repository dogfoods [abom](https://github.com/YanChao1999/abom) as an agent-material supply chain.

| File | Role |
|------|------|
| `abom.lock.json` | Pins installed recipe commits + material digests |
| `.abom/project.json` | Nearest-workspace wiring prefs (Cursor) |

## Local

```bash
pip install -U abom
abom sync
abom use abom-skill
abom lock --check
abom advisories
abom report abom-skill
```

## CI

`.github/workflows/abom.yml` installs `abom==0.0.4`, runs `sync` + `lock --check`, then `check` / `report` for every locked tool. The job fails on check failure or advisory hits.
