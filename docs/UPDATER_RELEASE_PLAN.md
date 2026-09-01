# RuneForge Realms — native updater and release plan

## Objective

RuneForge Realms should behave like a real installed game: it knows its version, can discover a newer release, stage it safely, switch versions atomically and roll back if launch validation fails.

The updater is **not Electron** and is not implemented inside the running game process.

## Components

### `RuneForgeBootstrap.exe`

Small native C++ executable responsible for reading version/channel, checking the release manifest, displaying minimal native progress/error UI, downloading, verifying, extracting to a new version directory, switching versions, launching RuneForge, recording health handshake, rollback and repair.

It contains no game-world code.

### `RuneForgeRealms.exe`

The game. It can request `restart-for-update`, but never overwrites its own running binaries.

## Install layout

```text
%LOCALAPPDATA%/RuneForgeRealms/
  bootstrap/
    RuneForgeBootstrap.exe
    bootstrap.json
  app/
    versions/
      0.1.0/
      0.1.1/
    current.json
    previous.json
  user/
    config/
    saves/
    screenshots/
    logs/
    crash/
    mods/
```

Critical rule: **saves/user data are outside versioned application folders**. Replacing a game version cannot delete a world.

## Release artifacts

GitHub Actions should produce a versioned package, manifest and SHA-256 file. Later add Windows code signing and release signatures.

Example manifest:

```json
{
  "product": "RuneForgeRealms",
  "version": "0.1.0",
  "channel": "stable",
  "minimumBootstrapVersion": "0.1.0",
  "platform": "windows-x64",
  "package": "RuneForgeRealms-0.1.0-win64.zip",
  "sha256": "...",
  "size": 123456789,
  "publishedAt": "...",
  "saveSchema": 1,
  "worldgenVersion": 1
}
```

Do not trust a package merely because HTTPS succeeded. Verify expected digest/signature.

## Update flow

```text
bootstrap starts
 -> read current version
 -> if offline/check disabled, launch installed game
 -> query configured release channel
 -> compare semantic versions
 -> download update to staging
 -> verify hash/signature
 -> extract to app/versions/<new>.staging
 -> validate expected files
 -> rename to final version directory
 -> atomically set pending current version
 -> launch new game with health token
 -> game reaches healthy main-menu checkpoint
 -> mark new version current + last-known-good
```

If the build repeatedly fails before health checkpoint:

```text
bootstrap detects crash loop
 -> restore previous.json
 -> launch last-known-good
 -> preserve failed version/logs for diagnosis
```

## Atomicity

Never update a live file in place. Use staging, verification before activation, atomic rename/pointer replacement and retain the previous version until the new version proves healthy.

## Save migrations

Application update and save migration are separate transactions.

Before opening an older schema:

1. inspect schema/version;
2. create backup;
3. migrate into temporary/new representation;
4. validate records/checksums;
5. commit migration;
6. retain backup.

Updater rollback does not automatically downgrade an already migrated save, so save migrations need explicit backward-compatibility/version policy.

## Channels

Initial channels:

- `dev` — internal frequent builds;
- `preview` — opt-in tests;
- `stable` — normal release.

Stable never silently jumps to preview/dev.

## GitHub release workflow

```text
configure
 -> compile tests
 -> unit/integration tests
 -> deterministic worldgen/save fixtures
 -> Release compile
 -> shader compile/validation
 -> asset cook
 -> package
 -> smoke launch test
 -> manifest/SHA-256
 -> upload workflow artifact
 -> tagged release publishes GitHub Release assets
```

Use semantic app versions. Save schema and worldgen version remain independent.

## Repair mode

- verify installed files;
- redownload missing/corrupt package;
- reinstall current version without touching user data;
- safe graphics mode;
- open logs;
- restore previous version when available.

## Offline behavior

No network is required to play a local world. Failed update check logs and then launches installed game normally. Updater is convenience/reliability, not DRM.

## Security

Minimum production requirements:

- TLS only;
- validate release host/domain;
- checksum every package;
- reject archive path traversal;
- reject manifest platform/product mismatch;
- decompression/file-count limits;
- never execute unverified staging binaries;
- code-sign public Windows executables when signing infrastructure exists;
- cryptographic release signatures later;
- least privilege/per-user install by default.

## Bootstrapper source boundaries

```text
updater/
  BootstrapMain.cpp
  UpdateController.cpp/.h
  ReleaseManifest.cpp/.h
  ReleaseClient.cpp/.h
  DownloadJob.cpp/.h
  PackageVerifier.cpp/.h
  PackageExtractor.cpp/.h
  VersionStore.cpp/.h
  AtomicSwitch.cpp/.h
  HealthHandshake.cpp/.h
  RollbackManager.cpp/.h
  RepairManager.cpp/.h
  BootstrapUi.cpp/.h
```

Networking is hidden behind `ReleaseClient`/`DownloadJob` so the library can be replaced.

## Acceptance tests

- kill updater at every stage and recover;
- power-loss simulation around version switch;
- corrupt ZIP rejected;
- wrong hash rejected;
- malicious `../` entry rejected;
- unavailable network still launches game;
- crash-loop rolls back;
- saves untouched by install/repair/rollback;
- disk-full failure leaves current version usable;
- Unicode/space install path works;
- N-1 -> N works and current N is not redownloaded;
- channel isolation works.

## First implementation scope

Version 1 only needs installed-version file, GitHub Release manifest lookup, package download, hash verification, versioned extraction, atomic switch, launch, previous-version rollback, simple native progress UI and logs.

Polish comes after failure modes are proven.
