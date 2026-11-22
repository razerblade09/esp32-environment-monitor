# Contributing

## Branches

- `main` always builds and is only updated through pull requests.
- Work on short-lived branches named `<type>/<short-description>`, for example
  `feat/bme280-driver` or `fix/pms5003-checksum`.

## Commit messages

This project uses [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <summary in imperative mood, max ~72 chars>

<body: what changed and why, wrapped at ~72 chars>
```

| Type | Use for |
|---|---|
| `feat` | New functionality |
| `fix` | Bug fix |
| `refactor` | Code change that neither fixes a bug nor adds a feature |
| `build` | Build system or dependency changes |
| `ci` | CI configuration |
| `docs` | Documentation only |
| `test` | Adding or fixing tests |
| `chore` | Tooling and housekeeping |
| `hw` | Schematic, PCB or BOM changes |

Rules of thumb:

- One logical change per commit, and every commit should build.
- The summary says *what*; the body says *why*.
- Suggested scopes: `board`, `i2c`, `bme280`, `scd4x`, `sgp40`, `pms5003`, `display`, `app`.

## Firmware checks

```
pio run -d firmware                      # build
clang-format -i --style=file:firmware/.clang-format <files>
```

CI runs the same build and a formatting check on every pull request.
