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

From an ESP-IDF terminal:

```
./firmware/build.sh                                          # build
clang-format -i --style=file:firmware/.clang-format <files>   # format
```

CI runs the same `build.sh` in the ESP-IDF container, plus a formatting check, on every pull request.

## Continuous integration and releases

| Workflow | Trigger | What it does |
|---|---|---|
| Firmware | push / PR touching `firmware/` | clang-format check, `build.sh` in the ESP-IDF container, uploads the `.bin` |
| Hardware | push / PR touching `hardware/` | KiCad ERC (fails on errors only), uploads ERC report and schematic PDF |
| Release | tag `vX.Y.Z` | builds firmware, publishes a GitHub Release with the `.bin`, its checksum and release notes |

To cut a release, from an up-to-date `main`:

```
git tag -a v0.2.0 -m "v0.2.0"
git push origin v0.2.0
```

Download the `.bin` from the Release page and write it at offset `0x0`
(`./firmware/flash.sh <port>`, or any esptool-based flasher).

`main` should be protected: require a pull request and passing checks
(Settings > Branches > Add branch ruleset).
