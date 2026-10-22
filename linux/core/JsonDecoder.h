#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QStringList>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>

namespace syrtis::core {
// Stable DecodingError classification and coding path, independent of Qt's
// permissive toString/toInteger defaults. A missing key belongs to its parent
// path, matching Swift's keyNotFound context.
struct DecodingError : std::runtime_error {
    QString kind;
    QStringList path;
    QString field;
    DecodingError(QString kind, QStringList path, QString field = {})
        : std::runtime_error(kind.toStdString()), kind(kind), path(path), field(field) {}
};

namespace json {
template<class T> struct Optional : std::false_type {};
template<class T> struct Optional<std::optional<T>> : std::true_type { using Value = T; };
template<class T> struct List : std::false_type {};
template<class T> struct List<QList<T>> : std::true_type { using Value = T; };

struct Decoder {
    QJsonValue value;
    QStringList path;
    [[noreturn]] void fail(const QString &kind) const { throw DecodingError(kind, path); }
    void object() const {
        if (value.isNull()) fail("valueNotFound");
        if (!value.isObject()) fail("typeMismatch");
    }
    template<class T> T field(const QString &name) const {
        object();
        const auto child = value.toObject().value(name);
        if constexpr (!Optional<T>::value)
            if (child.isUndefined()) throw DecodingError("keyNotFound", path, name);
        // JSONDecoder's keyed Bool overload checks the JSON type directly,
        // including null. Other required types use valueNotFound for null.
        // Keep this here: single-value decoding has its own null check.
        if constexpr (std::is_same_v<T, bool>)
            if (!child.isBool()) throw DecodingError("typeMismatch", path + QStringList{name});
        return Decoder{child, path + QStringList{name}}.decode<T>();
    }
    template<class T> T decode() const {
        if constexpr (Optional<T>::value) {
            if (value.isNull() || value.isUndefined()) return std::nullopt;
            return decode<typename Optional<T>::Value>();
        } else {
            if (value.isNull()) fail("valueNotFound");
            if constexpr (std::is_same_v<T, QString>) {
                if (!value.isString()) fail("typeMismatch");
                return value.toString();
            } else if constexpr (std::is_same_v<T, bool>) {
                if (!value.isBool()) fail("typeMismatch");
                return value.toBool();
            } else if constexpr (std::is_same_v<T, qint64> || std::is_same_v<T, quint64>) {
                if (!value.isDouble()) fail("typeMismatch");
                // Qt keeps signed 64-bit JSON integers losslessly. Do not
                // round them through double (e.g. 9007199254740993).
                const auto signedValue = value.toInteger(std::numeric_limits<qint64>::min());
                if (signedValue != std::numeric_limits<qint64>::min()
                    || value == QJsonValue(std::numeric_limits<qint64>::min())) {
                    if constexpr (std::is_unsigned_v<T>)
                        if (signedValue < 0) fail("dataCorrupted");
                    return static_cast<T>(signedValue);
                }
                if constexpr (std::is_unsigned_v<T>) {
                    const double n = value.toDouble();
                    if (n >= 0 && n < std::ldexp(1.0, 64) && std::trunc(n) == n)
                        return static_cast<T>(n);
                }
                fail("dataCorrupted");
            } else if constexpr (std::is_same_v<T, double>) {
                if (!value.isDouble()) fail("typeMismatch");
                return value.toDouble();
            } else if constexpr (List<T>::value) {
                if (!value.isArray()) fail("typeMismatch");
                T out;
                for (const auto &child : value.toArray())
                    out.append(Decoder{child, path + QStringList{QString("Index %1").arg(out.size())}}
                                   .decode<typename List<T>::Value>());
                return out;
            } else {
                return T::decode(*this);
            }
        }
    }
};

inline QJsonValue parse(const QByteArray &bytes)
{
    // QJsonDocument only accepts object/array roots. Wrapping permits the same
    // scalar/null top-level inputs as JSONDecoder without implementing JSON.
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson("[" + bytes + "]", &error);
    if (error.error != QJsonParseError::NoError || doc.array().size() != 1)
        throw DecodingError("dataCorrupted", {});
    return doc.array().first();
}
}
}
