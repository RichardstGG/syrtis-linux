#pragma once
#include "UsageAttribution.h"

namespace syrtis::core {
// Swift String equality and ordering use canonical Unicode scalar identity.
inline QString attributionKey(const QString &s) { return s.normalized(QString::NormalizationForm_C); }
inline bool attributionLess(const QString &a, const QString &b) { return attributionKey(a).toUcs4() < attributionKey(b).toUcs4(); }
inline QString attributionStateID(const UsageAttribution::State &s) {
    using Kind = UsageAttribution::State::Kind;
    return s.kind == Kind::assigned ? "assigned:" + s.target : s.kind == Kind::excluded ? "excluded" : "unassigned";
}
}
