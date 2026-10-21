#pragma once
#include "JsonDecoder.h"

namespace syrtis::core {
struct TBCoreError : std::runtime_error {
    QString message;
    explicit TBCoreError(const QString &message)
        : std::runtime_error(message.toStdString()), message(message) {}
};
struct ProbeResult {
    bool ok;
    std::optional<qint64> messages;
    std::optional<QString> err;
    static ProbeResult decode(const json::Decoder &decoder);
};
struct ScanPathNote {
    QString client;
    QString path;
    QString reason;
    static ScanPathNote decode(const json::Decoder &decoder);
};
struct ExtraScanPathsResult {
    qint64 registeredCount;
    QList<ScanPathNote> unreadable;
    QList<ScanPathNote> rejected;
    static ExtraScanPathsResult decode(const json::Decoder &decoder);
};
struct RejectedConfigDir {
    QString path;
    QString reason;
    static RejectedConfigDir decode(const json::Decoder &decoder);
};
struct ClaudeConfigDirsResult {
    qint64 registeredCount;
    QList<RejectedConfigDir> rejected;
    static ClaudeConfigDirsResult decode(const json::Decoder &decoder);
};
struct RejectedAntigravityAccount {
    qint64 index;
    QString reason;
    static RejectedAntigravityAccount decode(const json::Decoder &decoder);
};
struct AntigravityAccountsResult {
    qint64 registeredCount;
    QList<RejectedAntigravityAccount> rejected;
    static AntigravityAccountsResult decode(const json::Decoder &decoder);
};
struct AntigravityCapturedAccount {
    QString key;
    QString label;
    static AntigravityCapturedAccount decode(const json::Decoder &decoder);
};
struct AntigravityBindingResult {
    bool bound;
    static AntigravityBindingResult decode(const json::Decoder &decoder);
};
struct AntigravityRemoved {
    bool removed;
    static AntigravityRemoved decode(const json::Decoder &decoder);
};
struct AntigravityLoginMarker {
    QString marker;
    static AntigravityLoginMarker decode(const json::Decoder &decoder);
};
struct AntigravityAutoCaptureResult {
    QString status;
    std::optional<QString> key;
    std::optional<QString> label;
    static AntigravityAutoCaptureResult decode(const json::Decoder &decoder);
};
struct RejectedKeychainConsent {
    QString client;
    QString reason;
    static RejectedKeychainConsent decode(const json::Decoder &decoder);
};
struct KeychainConsentResult {
    qint64 grantedCount;
    QList<RejectedKeychainConsent> rejected;
    static KeychainConsentResult decode(const json::Decoder &decoder);
};
struct CursorSyncConfigResult {
    bool enabled;
    std::optional<QString> dir;
    bool cliTakeoverConfirmed;
    qint64 removedFiles;
    bool cleanupFailed;
    static CursorSyncConfigResult decode(const json::Decoder &decoder);
};
struct CursorSyncStatus {
    QString state;
    qint64 events;
    std::optional<qint64> lastSuccessMs;
    std::optional<QString> reason;
    static CursorSyncStatus decode(const json::Decoder &decoder);
};
// Pure DTO decoding. The bridge owns FFI calls and byte/pointer lifetime.
namespace TBCore {
template<class T> T decode(const QJsonValue &value) { return json::Decoder{value, {}}.decode<T>(); }
template<class T> T decode(const QByteArray &bytes) { return decode<T>(json::parse(bytes)); }
template<class T> struct TBEnvelope {
    bool ok;
    std::optional<T> data;
    std::optional<QString> err;
    bool hasDataKey;
    static TBEnvelope decode(const json::Decoder &d) {
        d.object();
        // Decode in Swift's declaration order, even for ok:false.
        return {d.field<bool>("ok"), d.field<std::optional<T>>("data"),
                d.field<std::optional<QString>>("err"), d.value.toObject().contains("data")};
    }
};
template<class T> std::optional<T> decodeOptionalEnvelope(const QJsonValue &value) {
    const auto envelope = decode<TBEnvelope<T>>(value);
    if (!envelope.ok) throw TBCoreError(envelope.err.value_or("unknown"));
    if (!envelope.hasDataKey)
        throw DecodingError("dataCorrupted", {}, "successful FFI envelope is missing the data key");
    return envelope.data;
}
template<class T> T decodeEnvelope(const QJsonValue &value) {
    const auto envelope = decode<TBEnvelope<T>>(value);
    if (!envelope.ok) throw TBCoreError(envelope.err.value_or("unknown"));
    if (!envelope.data)
        throw DecodingError("dataCorrupted", {}, "successful FFI envelope is missing data");
    return *envelope.data;
}
template<class T> T decodeEnvelope(const QByteArray &bytes) { return decodeEnvelope<T>(json::parse(bytes)); }
template<class T> std::optional<T> decodeOptionalEnvelope(const QByteArray &bytes) {
    return decodeOptionalEnvelope<T>(json::parse(bytes));
}
}
}
