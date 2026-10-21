#include "TBCore.h"

namespace syrtis::core {
ProbeResult ProbeResult::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<bool>("ok"),
        d.field<std::optional<qint64>>("messages"),
        d.field<std::optional<QString>>("err"),
    };
}
ScanPathNote ScanPathNote::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("client"),
        d.field<QString>("path"),
        d.field<QString>("reason"),
    };
}
ExtraScanPathsResult ExtraScanPathsResult::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<qint64>("registeredCount"),
        d.field<QList<ScanPathNote>>("unreadable"),
        d.field<QList<ScanPathNote>>("rejected"),
    };
}
RejectedConfigDir RejectedConfigDir::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("path"),
        d.field<QString>("reason"),
    };
}
ClaudeConfigDirsResult ClaudeConfigDirsResult::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<qint64>("registeredCount"),
        d.field<QList<RejectedConfigDir>>("rejected"),
    };
}
RejectedAntigravityAccount RejectedAntigravityAccount::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<qint64>("index"),
        d.field<QString>("reason"),
    };
}
AntigravityAccountsResult AntigravityAccountsResult::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<qint64>("registeredCount"),
        d.field<QList<RejectedAntigravityAccount>>("rejected"),
    };
}
AntigravityCapturedAccount AntigravityCapturedAccount::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("key"),
        d.field<QString>("label"),
    };
}
AntigravityBindingResult AntigravityBindingResult::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<bool>("bound"),
    };
}
AntigravityRemoved AntigravityRemoved::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<bool>("removed"),
    };
}
AntigravityLoginMarker AntigravityLoginMarker::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("marker"),
    };
}
AntigravityAutoCaptureResult AntigravityAutoCaptureResult::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("status"),
        d.field<std::optional<QString>>("key"),
        d.field<std::optional<QString>>("label"),
    };
}
RejectedKeychainConsent RejectedKeychainConsent::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("client"),
        d.field<QString>("reason"),
    };
}
KeychainConsentResult KeychainConsentResult::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<qint64>("grantedCount"),
        d.field<QList<RejectedKeychainConsent>>("rejected"),
    };
}
CursorSyncConfigResult CursorSyncConfigResult::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<bool>("enabled"),
        d.field<std::optional<QString>>("dir"),
        d.field<bool>("cliTakeoverConfirmed"),
        d.field<qint64>("removedFiles"),
        d.field<bool>("cleanupFailed"),
    };
}
CursorSyncStatus CursorSyncStatus::decode(const json::Decoder &d)
{
    d.object();
    return {
        d.field<QString>("state"),
        d.field<qint64>("events"),
        d.field<std::optional<qint64>>("lastSuccessMs"),
        d.field<std::optional<QString>>("reason"),
    };
}
}
