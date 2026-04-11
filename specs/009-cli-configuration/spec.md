# Feature Specification: CLI & Configuration

**Feature Branch**: `009-cli-configuration`  
**Created**: 2026-04-11  
**Status**: Draft  
**Input**: User description: "Implement 009 — CLI & Configuration"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Display Help and Usage Information (Priority: P1)

An operator launches the daemon with `--help` to learn what command-line options are available, what their defaults are, and how to configure the node.

**Why this priority**: Without discoverable usage information, operators cannot learn or use any of the other CLI features. This is the gateway to all other functionality.

**Independent Test**: Run the daemon with `--help` and verify that structured, readable help output is displayed listing all available options and their defaults, then the process exits cleanly without starting any services.

**Acceptance Scenarios**:

1. **Given** the daemon binary exists, **When** the operator runs it with `--help`, **Then** the daemon prints help text listing all command-line options with descriptions and default values, and exits with code 0.
2. **Given** the daemon binary exists, **When** the operator runs it with `--version`, **Then** the daemon prints the version string and exits with code 0.
3. **Given** the daemon binary exists, **When** the operator runs it with no arguments, **Then** the daemon prints a brief usage message indicating the required blockchain directory argument and exits with a non-zero code.

---

### User Story 2 - Override Configuration via Command-Line Flags (Priority: P1)

An operator wants to run the daemon with non-default ports or peer settings without editing `config.json`. They pass command-line flags that override the corresponding `config.json` values.

**Why this priority**: Command-line overrides are the core value of this feature — they enable quick testing, multi-instance setups, and containerized deployments where editing config files is inconvenient.

**Independent Test**: Start the daemon with `--rpc-port 9999 --p2p-port 9998` and verify it binds to those ports instead of the values in `config.json`.

**Acceptance Scenarios**:

1. **Given** `config.json` sets `rpc_port` to 12345, **When** the operator starts the daemon with `--rpc-port 9999`, **Then** the RPC server listens on port 9999.
2. **Given** `config.json` sets `p2p_port` to 12346, **When** the operator starts the daemon with `--p2p-port 9998`, **Then** the P2P server listens on port 9998.
3. **Given** the operator passes `--seed-node host1:12346 --seed-node host2:12346`, **When** the daemon starts, **Then** those seed nodes are included in the peer discovery list in addition to any seeds from `config.json`.
4. **Given** the operator passes `--log-level debug`, **When** the daemon starts, **Then** debug-level messages are emitted to the log output.
5. **Given** no command-line overrides are provided, **When** the daemon starts, **Then** all values are read from `config.json` (or defaults) as before.

---

### User Story 3 - Set Log Verbosity Level (Priority: P2)

An operator wants to control how much log output the daemon produces — verbose for debugging, quiet for production.

**Why this priority**: Log level control is essential for both debugging in development and reducing noise in production. It depends on the CLI flag infrastructure from Story 2.

**Independent Test**: Start the daemon with `--log-level error`, perform normal operations, and verify that only error-level messages appear in the output.

**Acceptance Scenarios**:

1. **Given** the operator starts with `--log-level debug`, **When** normal operations occur, **Then** debug, info, warning, and error messages are all emitted.
2. **Given** the operator starts with `--log-level info` (or no flag), **When** normal operations occur, **Then** info, warning, and error messages are emitted, but debug messages are suppressed.
3. **Given** the operator starts with `--log-level error`, **When** non-critical events occur, **Then** only error messages are emitted.
4. **Given** an invalid log level like `--log-level banana` is provided, **When** the daemon starts, **Then** a clear error message is displayed and the daemon exits with a non-zero code.

---

### User Story 4 - Generate Default Configuration File (Priority: P2)

An operator setting up a new node wants to generate a well-commented default `config.json` in their blockchain directory so they can review and customize it before first launch.

**Why this priority**: Improves onboarding for new operators. The daemon already auto-generates a default config if none exists, but an explicit command gives the operator more control and visibility.

**Independent Test**: Run the daemon with `--generate-config` pointing at an empty directory, verify a valid `config.json` is created, then start the daemon with that config and confirm it boots successfully.

**Acceptance Scenarios**:

1. **Given** a blockchain directory with no `config.json`, **When** the operator runs the daemon with `--generate-config`, **Then** a default `config.json` is written to the blockchain directory and the daemon exits with code 0.
2. **Given** a blockchain directory that already contains a `config.json`, **When** the operator runs `--generate-config`, **Then** the daemon refuses to overwrite the existing file, displays a warning, and exits with a non-zero code.
3. **Given** a freshly generated `config.json`, **When** the operator inspects it, **Then** it contains all configuration sections (TLS, network, consensus, peers, streams, persistence) with sensible defaults.
4. **Given** `--generate-config` was run, **When** the operator lists the blockchain directory, **Then** a companion `config.README` file is present alongside `config.json` describing each field, its type, valid values, and default.

---

### User Story 5 - Validate Configuration at Startup (Priority: P3)

An operator with a hand-edited `config.json` wants the daemon to catch configuration errors early, with clear messages, rather than failing deep into initialization.

**Why this priority**: Validation already exists partially, but expanding it with clear error messages improves operator experience and reduces debugging time.

**Independent Test**: Start the daemon with a `config.json` containing an invalid port value (e.g., `"rpc_port": 99999`), verify the daemon exits immediately with a clear, actionable error message.

**Acceptance Scenarios**:

1. **Given** `config.json` contains `rpc_port` set to 99999 (out of valid range), **When** the daemon starts, **Then** it prints an error identifying the invalid field and valid range, and exits with a non-zero code.
2. **Given** `config.json` contains `rpc_port` and `p2p_port` set to the same value, **When** the daemon starts, **Then** it prints an error about the port conflict and exits with a non-zero code.
3. **Given** `config.json` references a TLS certificate file that does not exist, **When** the daemon starts, **Then** it prints an error identifying the missing file and exits with a non-zero code.
4. **Given** a valid `config.json` with all fields properly set, **When** the daemon starts, **Then** configuration validation passes and the daemon proceeds to normal startup.

---

### Edge Cases

- What happens when a command-line flag and `config.json` both specify the same setting? Command-line flags always take precedence over file-based configuration.
- What happens when the blockchain directory path doesn't exist? The daemon should print a clear error and exit, not create the directory implicitly.
- What happens when `config.json` contains unknown or deprecated keys? The daemon should log a warning for unrecognized keys but continue startup (forward compatibility).
- What happens when the operator provides a flag that expects a value but omits it (e.g., `--rpc-port` with no number)? The CLI parser should display a specific error for the missing value.
- What happens when a port number is already in use by another process? The daemon should catch the bind error and report which port/service conflicted.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The daemon MUST accept a `--help` (or `-h`) flag that prints structured usage information listing all available options, their descriptions, and default values, then exits with code 0.
- **FR-002**: The daemon MUST accept a `--version` (or `-v`) flag that prints the program name and version, then exits with code 0.
- **FR-003**: The daemon MUST accept a `--rpc-port <port>` flag that overrides the `network.rpc_port` value from `config.json`.
- **FR-004**: The daemon MUST accept a `--p2p-port <port>` flag that overrides the `network.p2p_port` value from `config.json`.
- **FR-005**: The daemon MUST accept one or more `--seed-node <host:port>` flags that add seed nodes to the peer discovery list alongside any seeds from `config.json`.
- **FR-006**: The daemon MUST accept a `--log-level <level>` flag accepting values `debug`, `info`, `warning`, and `error` (case-insensitive), controlling log verbosity.
- **FR-007**: The daemon MUST default the log level to `info` when no `--log-level` flag is provided and no `log_level` key is set in `config.json`. The `log_level` field is part of the `network` section in `config.json`, accepting the same values as the CLI flag (`debug`, `info`, `warning`, `error`).
- **FR-008**: The daemon MUST accept a `--generate-config` flag that writes a default `config.json` and a companion `config.README` (documenting each field, its type, valid values, and default) to the blockchain directory and exits, refusing to overwrite an existing `config.json`.
- **FR-009**: The daemon MUST apply configuration precedence in the following order (highest to lowest): command-line flags → `config.json` values → built-in defaults.
- **FR-010**: The daemon MUST validate all configuration values at startup before binding ports or connecting to peers, and MUST report all validation errors (not just the first) with field name and reason before exiting.
- **FR-011**: The daemon MUST validate that port numbers are in the range 1–65535 and that `rpc_port` and `p2p_port` are not equal.
- **FR-012**: The daemon MUST validate that referenced TLS certificate and key files exist on disk.
- **FR-013**: The daemon MUST log a warning for any unrecognized keys in `config.json` and continue startup.
- **FR-014**: The daemon MUST continue to accept the blockchain directory as a required positional argument.
- **FR-015**: The daemon MUST accept a `--config <path>` flag that specifies an alternative path to `config.json` instead of the default location inside the blockchain directory.
- **FR-016**: The daemon MUST exit with a clear error message if the blockchain directory does not exist.

### Key Entities

- **Command-Line Arguments**: The set of flags and positional arguments accepted by the daemon, parsed before any config file is read.
- **Configuration Precedence Chain**: The layered system where CLI flags override file values, which override built-in defaults.
- **Log Level**: A runtime verbosity setting (`debug`, `info`, `warning`, `error`) that filters which messages reach the output. Persistable via `network.log_level` in `config.json`; overridable via `--log-level` CLI flag.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Operators can discover all available configuration options by running a single help command without consulting external documentation.
- **SC-002**: Operators can override any network port setting via command-line flags without editing any files.
- **SC-003**: Operators can adjust log verbosity to any of the four defined levels and only see messages at or above the selected severity.
- **SC-004**: Configuration errors in `config.json` are detected and reported with actionable messages before the daemon opens any network connections.
- **SC-005**: A new operator can generate, inspect, and launch with a default configuration in under one minute.
- **SC-006**: All existing `config.json` files from previous versions continue to work without modification (backward compatible).

## Clarifications

### Session 2026-04-11

- Q: Should `log_level` be added to the config.json schema or remain CLI-only? → A: Add `log_level` to config.json under the `network` section (persistable + CLI override).
- Q: What format should the version string follow and where does it live? → A: Derived from `configure.ac` via `config.h` (Autoconf `PACKAGE_VERSION` / `VERSION` macros).
- Q: How should the generated default config.json be documented given JSON has no comments? → A: Generate a companion `config.README` file alongside `config.json` with field descriptions.
- Q: Should there be a flag to disable config-file seed nodes for isolated testing? → A: No extra flag; operators use `--config` pointing to a config with an empty seed list.

## Assumptions

- The existing `config.json` schema and `NodeConfig` class are the foundation; this feature extends them rather than replacing them.
- The version string is the `PACKAGE_VERSION` macro from `config.h`, set by the `AC_INIT` directive in `configure.ac`. No separate version constant is needed.
- Log output goes to standard error (stderr), consistent with the existing `logMessage` function behavior.
- There is no need for a daemon/background mode (`--daemonize`) in this iteration; the process runs in the foreground.
- Environment variable overrides are out of scope for this iteration; the precedence chain is CLI → file → defaults only.
- Hot-reloading of `config.json` at runtime is out of scope; configuration is read once at startup.
