# Research: CLI & Configuration

**Feature**: 009-cli-configuration
**Date**: 2026-04-11

## Research Task 1: Cross-Platform CLI Parsing (Constitution §VII)

### Problem

`getopt_long` is the natural C/POSIX CLI parsing API and ships with glibc (Linux) and macOS. However, Windows MSVC does not provide `getopt.h`. Constitution §V prohibits adding unapproved dependencies, and §VII requires cross-platform builds.

### Options Evaluated

| Option | Pros | Cons |
|--------|------|------|
| A. `getopt_long` + bundled Windows shim | Zero new deps on Linux/macOS; small (~150 LOC) MIT-licensed shim for Windows | Must vendor a shim file; shim maintenance burden |
| B. Boost.ProgramOptions | Already approved (Boost is in dependency list); fully cross-platform | Heavier; adds link-time dependency (`-lboost_program_options`); different API style from rest of codebase |
| C. Manual `argv` parsing | No dependencies at all; full control | Error-prone; tedious to maintain; poor `--help` generation |
| D. Header-only CLI library (e.g., CLI11, cxxopts) | Modern; header-only; cross-platform | New dependency requiring constitution approval |

### Decision: Option B — Boost.ProgramOptions

**Rationale**: Boost is already an approved dependency (constitution §V). `Boost.ProgramOptions` is part of the Boost distribution and provides portable, well-tested CLI parsing with automatic `--help` generation. The link-time addition (`-lboost_program_options`) is trivial since Boost is already required. This avoids vendoring a `getopt` shim and dodges the maintenance burden of manual parsing.

**Alternatives rejected**:
- Option A rejected because vendoring a Windows shim adds maintenance and the shim quality varies.
- Option C rejected because manual parsing is fragile and produces poor help output.
- Option D rejected because it would require a constitution amendment for a new dependency.

### Integration Notes

- Add `AX_BOOST_PROGRAM_OPTIONS` to `configure.ac` (Autotools macro already exists in the Boost m4 collection).
- Add `-lboost_program_options` to `blockchain_LDADD` in `src/Makefile.am`.
- Test binary `cli_tests` also needs the link flag if it tests `CliParser` directly.

---

## Research Task 2: Log Level Filtering Best Practices

### Problem

The existing `logMessage(level, msg)` function writes all messages unconditionally to stderr. Need to add filtering without breaking existing call sites or changing the function signature in a breaking way.

### Options Evaluated

| Option | Pros | Cons |
|--------|------|------|
| A. Global `LogLevel` variable + filter in `logMessage` | Minimal change; all existing callers unchanged; single point of control | Global state; not thread-safe without atomic |
| B. Logger class with level member | Encapsulated; testable; injectable | Requires changing all call sites or adding a global instance (same as A) |
| C. Preprocessor macros | Zero runtime cost for disabled levels | Hard to change at runtime; non-idiomatic C++20 |

### Decision: Option A — Global atomic LogLevel + filter in logMessage

**Rationale**: The existing codebase uses a free function `logMessage` called from dozens of sites. Changing all call sites to use a logger object would be a large, risky diff that isn't required by the spec. A global `std::atomic<LogLevel>` is thread-safe (Boost.Asio handlers may log from different threads) and requires zero changes to existing callers. The only modification to `logMessage` is an early-return check.

**Implementation**:
- Add `enum class LogLevel { Debug, Info, Warning, Error }` to `utils.hpp`.
- Add `void setLogLevel(LogLevel)` and `LogLevel getLogLevel()` to `utils.hpp`.
- In `utils.cpp`, add `static std::atomic<LogLevel> g_log_level{LogLevel::Info}`.
- In `logMessage`, map the string level parameter to `LogLevel` and return early if below threshold.
- Add a `"DEBUG"` color (e.g., dim/default) to the existing color table.

### Level Mapping

| String (existing) | Enum | Numeric |
|-------------------|------|---------|
| `"DEBUG"` | `LogLevel::Debug` | 0 |
| `"INFO"` | `LogLevel::Info` | 1 |
| `"WARN"` | `LogLevel::Warning` | 2 |
| `"ERROR"` | `LogLevel::Error` | 3 |

---

## Research Task 3: config.json Unknown Key Detection

### Problem

FR-013 requires warning about unrecognized keys. The existing `NodeConfig::load` uses nlohmann/json and iterates known keys. Need to detect keys outside the known set.

### Decision: Diff known keys against parsed JSON keys

**Rationale**: After parsing, iterate top-level and second-level keys in the JSON object. Compare against a `std::set<std::string>` of known keys per section. Any key not in the known set triggers a `logMessage("WARN", ...)`. This is simple, requires no new dependencies, and is forward-compatible (new config versions just add to the known set).

---

## Research Task 4: Validation Enhancement

### Problem

FR-010 requires collecting all validation errors before exiting. The existing `NodeConfig::validate()` throws on the first error.

### Decision: Accumulate errors in a vector, throw after all checks

**Rationale**: Change `validate()` to collect errors in a `std::vector<std::string>`. After all checks, if the vector is non-empty, join the messages and throw a single `std::invalid_argument` containing all errors. This matches FR-010's requirement to report all errors, not just the first.

**New validations to add**:
- Port range 1–65535 (FR-011)
- Port conflict: `rpc_port != p2p_port` (FR-011)
- TLS cert/key file existence on disk (FR-012), resolved relative to blockchain directory

---

## Research Task 5: Version String Source

### Problem

FR-002 requires `--version` output. The version string must come from a single authoritative source.

### Decision: Use `PACKAGE_VERSION` from `config.h`

**Rationale**: `configure.ac` already defines `AC_INIT([blockchain], [0.0.1], ...)`. Autoconf generates `config.h` with `#define PACKAGE_VERSION "0.0.1"` and `#define PACKAGE_NAME "blockchain"`. The `--version` output will be `PACKAGE_NAME " " PACKAGE_VERSION` (e.g., `blockchain 0.0.1`). No new version constant needed.

**Note**: `config.h` must be included in the file that prints the version (either `main.cpp` or `CliParser.cpp`). Since `config.h` is project-root-relative, include it as `#include "../config.h"` from `src/` or adjust include paths.
