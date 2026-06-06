#pragma once

namespace Omnix
{
    enum class WorldFileError
    {
        None,

        FileNotFound,
        FileOpenFailed,
        FileReadFailed,
        FileWriteFailed,

        FileTooSmall,
        InvalidMagic,
        UnsupportedVersion,
        InvalidHeaderSize,
        InvalidOffset,
        InvalidWorldName,
        ZoneCountTooLarge,
        DependencyCountTooLarge,

        TruncatedWorldSettings,
        TruncatedEntryPoint,
        TruncatedZoneTable,
        TruncatedDependencyTable,

        ChecksumMismatch,
        CorruptedFile
    };
}
