# OMNIXWORLD Format Specification

This document defines the binary layout and specification for the `.omnixworld` format, which serves as the top-level world container descriptor for Omnix Engine game projects.

## Purpose
The `.omnixworld` file acts as the root world asset. Instead of storing every scene entity directly, it links zones and dependencies, configures world settings (like gravity and time-scales), and defines the initial entry point zone and spawning coordinates.

## File Extension
The standard file extension is:
```txt
.omnixworld
```

## Magic Value
The first 8 bytes of the file must match the magic signature:
```txt
OMXWORLD (expressed in Hex: 4F 4D 58 57 4F 52 4C 44)
```

## Versioning
An unsigned 32-bit integer indicates the format version.
* Current major version: `1`
* Readers must cleanly reject any file with a version greater than `1` with the error `WorldFileError::UnsupportedVersion`.

---

## Binary Layout
The file is structured as a sequential list of offset-jumpable blocks:
```txt
[OmnixWorldHeader]              <- Fixed size (256 bytes)
[WorldSettingsBlock]            <- Fixed size (96 bytes)
[EntryPointMetadataBlock]       <- Fixed size (416 bytes)
[ZoneTable]                     <- Array of WorldZoneEntry (396 bytes each)
[DependencyTable]               <- Array of WorldDependencyEntry (272 bytes each)
[Checksum]                      <- CRC32 value (4 bytes)
```

---

## Header Structure
The header is a fixed-size `256` byte block containing offsets pointing to other blocks, allowing parallel loading or selective parsing.

| Offset (Bytes) | Type | Name | Description |
|---|---|---|---|
| 0 | `char[8]` | magic | Must be `OMXWORLD` |
| 8 | `uint32_t` | version | Format version (Currently 1) |
| 12 | `uint64_t` | worldUUIDHigh | Identity UUID high 64-bits |
| 20 | `uint64_t` | worldUUIDLow | Identity UUID low 64-bits |
| 28 | `char[128]` | worldName | UTF-8 Null-terminated string |
| 156 | `uint32_t` | zoneCount | Number of zones in the zone table |
| 160 | `uint64_t` | worldSettingsOffset | Byte offset to `WorldSettingsBlock` |
| 168 | `uint64_t` | entryPointOffset | Byte offset to `WorldEntryPoint` |
| 176 | `uint64_t` | zoneTableOffset | Byte offset to `ZoneTable` |
| 184 | `uint64_t` | dependencyTableOffset | Byte offset to `DependencyTable` |
| 192 | `uint32_t` | dependencyCount | Number of entries in the dependency table |
| 196 | `uint64_t` | checksumOffset | Byte offset to CRC32 checksum field |
| 204 | `uint64_t` | fileSize | Total size of the file in bytes |
| 212 | `uint32_t` | headerSize | Header block size (256 bytes) |
| 216 | `uint32_t[16]` | reserved | Padded space reserved for future offsets |

---

## World Settings Block
Located at `worldSettingsOffset`. Contains physics parameters and system flags.

* **Size**: `96` bytes.
* **Fields**:
  * `float gravityX`, `float gravityY`, `float gravityZ` (Defaults: `0.0f`, `-9.81f`, `0.0f`)
  * `float worldTimeScale` (Default: `1.0f`)
  * `uint32_t enableStreaming` (Boolean `0` or `1`)
  * `uint32_t enablePhysics` (Boolean `0` or `1`)
  * `uint32_t enableNavigation` (Boolean `0` or `1`)
  * `uint32_t enableAudio` (Boolean `0` or `1`)
  * `uint32_t[16] reserved` (Padding)

---

## Entry Point Metadata
Located at `entryPointOffset`. Identifies the player start parameters.

* **Size**: `416` bytes.
* **Fields**:
  * `char entryZonePath[256]` (Null-terminated path to the initial `.omnixzone` file)
  * `float spawnPositionX`, `float spawnPositionY`, `float spawnPositionZ`
  * `float spawnRotationPitch`, `float spawnRotationYaw`, `float spawnRotationRoll`
  * `char spawnTag[64]` (Identifier matches player spawning coordinates in the scene)
  * `uint32_t[16] reserved` (Padding)

---

## Zone Table
Located at `zoneTableOffset`. An array of `zoneCount` entries representing the zones making up the world.

* **Entry Size**: `396` bytes.
* **Fields**:
  * `uint64_t zoneUUIDHigh`, `uint64_t zoneUUIDLow`
  * `char zoneName[128]`
  * `char zonePath[256]`
  * `uint32_t flags`

---

## Dependency Table
Located at `dependencyTableOffset`. An array of `dependencyCount` entries representing external assets.

* **Entry Size**: `272` bytes.
* **Fields**:
  * `uint64_t assetUUIDHigh`, `uint64_t assetUUIDLow`
  * `char assetPath[256]`
  * `uint32_t assetType`

---

## Checksum
Located at `checksumOffset`.
* **Type**: `uint32_t` (4 bytes).
* **Algorithm**: Standard CRC32 (`0xEDB88320` polynomial).
* **Scope**: Calculates hash of all bytes in the file, treating the 4 bytes at `checksumOffset` as 0.

---

## Reader Validation Rules
The reader must strictly validate the container prior to loading:
1. File size must be greater than or equal to `sizeof(OmnixWorldHeader)` (256 bytes).
2. The first 8 bytes must match `OMXWORLD`.
3. The format version must not exceed `1`.
4. Header size must match exactly `256` bytes.
5. All block offsets (`worldSettingsOffset`, `entryPointOffset`, `zoneTableOffset`, `dependencyTableOffset`, `checksumOffset`) must be within the file bounds: `offset + blockSize <= fileSize`.
6. The checksum calculated by treating checksum bytes as `0` must match the `uint32_t` value stored at `checksumOffset`.
7. Reject files with `zoneCount > 4096` or `dependencyCount > 65536`.

---

## Writer Rules
1. Serialize the fixed header first with placeholder values.
2. Maintain sequential block alignment.
3. Record exact file offsets as they are written.
4. Calculate final checksum over the serialized stream, writing the resulting CRC32 back to `checksumOffset`.
5. Ensure binary output is fully deterministic. Same inputs must yield bitwise identical binary files.

---

## Error Codes
Enum mappings in `WorldFileError`:
* `None` — Success
* `FileNotFound` — File path does not exist
* `FileOpenFailed` — File exists but cannot be opened
* `FileReadFailed` — File IO stream read error
* `FileWriteFailed` — File IO stream write error
* `FileTooSmall` — File size is less than 256 bytes
* `InvalidMagic` — First 8 bytes mismatch `OMXWORLD`
* `UnsupportedVersion` — Version > 1
* `InvalidHeaderSize` — Header size mismatch
* `InvalidOffset` — Offset lies outside of file bounds
* `ZoneCountTooLarge` — Zone count exceeds 4096 entries
* `DependencyCountTooLarge` — Dependency count exceeds 65536 entries
* `ChecksumMismatch` — Corrupted payload detected
* `CorruptedFile` — Undefined format corruption

---

## Future Extensions
* **World Partitioning Grid**: References to partition sizes and boundary bounds.
* **Streaming Grids**: Pre-computed sector dependencies.
* **Lighting Scenarios**: Dynamic day/night cycle package settings.
* **MP Spawn Regions**: Multi-zone network player distribution rules.
