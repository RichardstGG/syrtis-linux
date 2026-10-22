#include "ClientRegistry.h"

#include <QtCore/QTextBoundaryFinder>
#include <algorithm>

namespace syrtis::core::ClientRegistry {
namespace {
const QHash<QString, ClientStyle> entries{
    {QStringLiteral("claude"), {QStringLiteral("claude"), QStringLiteral("Claude Code"), QStringLiteral("#d97706")}},
    {QStringLiteral("openclaw"), {QStringLiteral("openclaw"), QStringLiteral("OpenClaw"), QStringLiteral("#dc2626")}},
    {QStringLiteral("gemini"), {QStringLiteral("gemini"), QStringLiteral("Gemini CLI"), QStringLiteral("#60a5fa")}},
    {QStringLiteral("opencode"), {QStringLiteral("opencode"), QStringLiteral("OpenCode"), QStringLiteral("#1f2937")}},
    {QStringLiteral("codex"), {QStringLiteral("codex"), QStringLiteral("Codex"), QStringLiteral("#9ca3af")}},
    {QStringLiteral("copilot"), {QStringLiteral("copilot"), QStringLiteral("Copilot"), QStringLiteral("#1f2937")}},
    {QStringLiteral("cursor"), {QStringLiteral("cursor"), QStringLiteral("Cursor"), QStringLiteral("#0ea5e9")}},
    {QStringLiteral("amp"), {QStringLiteral("amp"), QStringLiteral("Amp"), QStringLiteral("#10b981")}},
    {QStringLiteral("droid"), {QStringLiteral("droid"), QStringLiteral("Droid"), QStringLiteral("#22c55e")}},
    {QStringLiteral("hermes"), {QStringLiteral("hermes"), QStringLiteral("Hermes"), QStringLiteral("#a78bfa")}},
    {QStringLiteral("pi"), {QStringLiteral("pi"), QStringLiteral("Pi"), QStringLiteral("#f472b6")}},
    {QStringLiteral("kimi"), {QStringLiteral("kimi"), QStringLiteral("Kimi"), QStringLiteral("#fbbf24")}},
    {QStringLiteral("junie"), {QStringLiteral("junie"), QStringLiteral("Junie"), QStringLiteral("#6b7280")}},
    {QStringLiteral("opencodereview"), {QStringLiteral("opencodereview"), QStringLiteral("OpenCodeReview"), QStringLiteral("#6b7280")}},
    {QStringLiteral("qwen"), {QStringLiteral("qwen"), QStringLiteral("Qwen CLI"), QStringLiteral("#7c3aed")}},
    {QStringLiteral("roocode"), {QStringLiteral("roocode"), QStringLiteral("Roo Code"), QStringLiteral("#ef4444")}},
    {QStringLiteral("kilocode"), {QStringLiteral("kilocode"), QStringLiteral("KiloCode"), QStringLiteral("#f97316")}},
    {QStringLiteral("kilo"), {QStringLiteral("kilo"), QStringLiteral("Kilo CLI"), QStringLiteral("#f59e0b")}},
    {QStringLiteral("mux"), {QStringLiteral("mux"), QStringLiteral("Mux"), QStringLiteral("#06b6d4")}},
    {QStringLiteral("crush"), {QStringLiteral("crush"), QStringLiteral("Crush"), QStringLiteral("#ec4899")}},
    {QStringLiteral("synthetic"), {QStringLiteral("synthetic"), QStringLiteral("Synthetic"), QStringLiteral("#64748b")}},
    {QStringLiteral("goose"), {QStringLiteral("goose"), QStringLiteral("Goose"), QStringLiteral("#14b8a6")}},
    {QStringLiteral("codebuff"), {QStringLiteral("codebuff"), QStringLiteral("Codebuff"), QStringLiteral("#8b5cf6")}},
    {QStringLiteral("antigravity"), {QStringLiteral("antigravity"), QStringLiteral("Antigravity"), QStringLiteral("#3b82f6")}},
    {QStringLiteral("zed"), {QStringLiteral("zed"), QStringLiteral("Zed"), QStringLiteral("#084fff")}},
    {QStringLiteral("kiro"), {QStringLiteral("kiro"), QStringLiteral("Kiro"), QStringLiteral("#9046ff")}},
    {QStringLiteral("trae"), {QStringLiteral("trae"), QStringLiteral("Trae"), QStringLiteral("#ef4444")}},
    {QStringLiteral("warp"), {QStringLiteral("warp"), QStringLiteral("Warp"), QStringLiteral("#01a4ff")}},
    {QStringLiteral("cline"), {QStringLiteral("cline"), QStringLiteral("Cline"), QStringLiteral("#5b8def")}},
    {QStringLiteral("antigravity-cli"), {QStringLiteral("antigravity-cli"), QStringLiteral("Antigravity CLI"), QStringLiteral("#6366f1")}},
    {QStringLiteral("jcode"), {QStringLiteral("jcode"), QStringLiteral("Jcode"), QStringLiteral("#84cc16")}},
    {QStringLiteral("micode"), {QStringLiteral("micode"), QStringLiteral("MiMo Code"), QStringLiteral("#fb923c")}},
    {QStringLiteral("gjc"), {QStringLiteral("gjc"), QStringLiteral("gjc"), QStringLiteral("#e11d48")}},
    {QStringLiteral("grok"), {QStringLiteral("grok"), QStringLiteral("Grok Build"), QStringLiteral("#1f2937")}},
    {QStringLiteral("grok-bot"), {QStringLiteral("grok-bot"), QStringLiteral("Grok Bot"), QStringLiteral("#000000")}},
    {QStringLiteral("zcode"), {QStringLiteral("zcode"), QStringLiteral("ZCode"), QStringLiteral("#3b5bdb")}},
    {QStringLiteral("augment"), {QStringLiteral("augment"), QStringLiteral("Augment Code"), QStringLiteral("#9333ea")}},
    {QStringLiteral("hindsight"), {QStringLiteral("hindsight"), QStringLiteral("Hindsight"), QStringLiteral("#0891b2")}},
    {QStringLiteral("muse"), {QStringLiteral("muse"), QStringLiteral("Muse Code"), QStringLiteral("#0064e0")}},
    {QStringLiteral("reasonix"), {QStringLiteral("reasonix"), QStringLiteral("Reasonix"), QStringLiteral("#808000")}},
    {QStringLiteral("kimchi"), {QStringLiteral("kimchi"), QStringLiteral("Kimchi"), QStringLiteral("#7f1d1d")}},
    {QStringLiteral("senpi"), {QStringLiteral("senpi"), QStringLiteral("Senpi"), QStringLiteral("#2f6f63")}},
    {QStringLiteral("omp"), {QStringLiteral("omp"), QStringLiteral("Oh My Pi"), QStringLiteral("#d946ef")}},
};
struct TabGroup { QStringList members; QString label; };
const QHash<QString, TabGroup> tabGroups{
    {QStringLiteral("grok"), {{QStringLiteral("grok"), QStringLiteral("grok-bot")}, QStringLiteral("Grok Build & Bot")}},
    {QStringLiteral("antigravity"), {{QStringLiteral("antigravity"), QStringLiteral("antigravity-cli")}, QStringLiteral("Antigravity")}},
};
QString tabId(const QString &id)
{
    for (auto it = tabGroups.cbegin(); it != tabGroups.cend(); ++it)
        if (it->members.contains(id)) return it.key();
    return id;
}
QStringList unique(const QStringList &ids)
{
    QStringList out;
    QSet<QString> seen;
    for (const auto &id : ids)
        if (!seen.contains(id)) { seen.insert(id); out.append(id); }
    return out;
}
QStringList ordered(const QStringList &ids, const QStringList &order)
{
    if (order.isEmpty()) return ids;
    auto out = ids;
    std::stable_sort(out.begin(), out.end(), [&](const auto &a, const auto &b) {
        const auto ia = order.indexOf(a), ib = order.indexOf(b);
        if (ia == ib) return false;
        if (ia < 0) return false;
        if (ib < 0) return true;
        return ia < ib;
    });
    return out;
}
}
const QHash<QString, QString> subscriptionLabelAliases{
    {QStringLiteral("Codex"), QStringLiteral("codex")},
    {QStringLiteral("Claude"), QStringLiteral("claude")},
    {QStringLiteral("Copilot"), QStringLiteral("copilot")},
    {QStringLiteral("Gemini"), QStringLiteral("antigravity")},
};
QStringList allIds()
{
    auto ids = entries.keys();
    ids.sort();
    return ids;
}
ClientStyle style(const QString &id)
{
    const auto it = entries.constFind(id);
    if (it != entries.cend()) return *it;
    QTextBoundaryFinder grapheme(QTextBoundaryFinder::Grapheme, id);
    const auto end = grapheme.toNextBoundary();
    return {id, end < 0 ? id : id.left(end).toUpper() + id.mid(end), QStringLiteral("#6b7280")};
}
QString shortName(const QString &id)
{
    const auto name = style(id).displayName;
    for (const auto &suffix : {QStringLiteral(" CLI"), QStringLiteral(" Code")}) {
        if (!name.endsWith(suffix)) continue;
        const auto base = name.chopped(suffix.size());
        const bool registered = std::any_of(entries.cbegin(), entries.cend(),
            [&](const auto &entry) { return entry.displayName == base; });
        if (!registered) return base;
    }
    return name;
}
QString canonicalClient(const QString &id)
{
    if (id == QStringLiteral("claude-code")) return QStringLiteral("claude");
    if (id == QStringLiteral("codex-cli")) return QStringLiteral("codex");
    if (id == QStringLiteral("gemini-cli")) return QStringLiteral("gemini");
    return id;
}
QString quotaOwner(const QString &id)
{
    return id == QStringLiteral("antigravity-cli") ? QStringLiteral("antigravity") : id;
}
QString clientIdForSubscriptionLabel(const QString &label)
{
    return subscriptionLabelAliases.value(label, label.toLower());
}
QStringList parseIdList(const QString &raw) { return raw.split(',', Qt::SkipEmptyParts); }
QSet<QString> parseIdSet(const QString &raw)
{
    const auto list = parseIdList(raw);
    return {list.cbegin(), list.cend()};
}
QSet<QString> hiddenClients(const ClientPreferences &p) { return parseIdSet(p.hidden); }
QSet<QString> hiddenLimitsClients(const ClientPreferences &p) { return parseIdSet(p.limitsHidden); }
QSet<QString> quotaExcludedClients(const QSet<QString> &tabHidden, const QSet<QString> &limitsHidden)
{
    return hiddenTabClients(tabHidden).unite(limitsHidden);
}
QSet<QString> quotaExcludedClients(const ClientPreferences &p)
{
    return quotaExcludedClients(hiddenClients(p), hiddenLimitsClients(p));
}
QStringList tabSlice(const QString &id)
{
    const auto it = tabGroups.constFind(id);
    return it == tabGroups.cend() ? QStringList{id} : it->members;
}
QStringList tabClients(const QStringList &present, const QStringList &quotaIds)
{
    QStringList out;
    for (const auto &id : present + quotaIds) out.append(tabId(id));
    return unique(out);
}
QStringList quotaClients(const QStringList &present, const QStringList &quotaIds,
                         const QSet<QString> &tabHidden, const QString &orderRaw)
{
    QStringList ids;
    for (const auto &id : present) ids.append(tabSlice(quotaOwner(id)));
    ids.append(quotaIds);
    const auto hidden = hiddenTabClients(tabHidden);
    ids.removeIf([&](const auto &id) { return hidden.contains(id); });
    return orderedClients(unique(ids), orderRaw);
}
QString tabLabel(const QString &id)
{
    const auto it = tabGroups.constFind(id);
    return it == tabGroups.cend() ? shortName(id) : it->label;
}
QString tabDisplayName(const QString &id)
{
    return tabSlice(id).size() > 1 ? tabLabel(id) : style(id).displayName;
}
QSet<QString> withGroupMembers(const QSet<QString> &ids)
{
    auto out = ids;
    for (const auto &id : ids)
        if (tabGroups.contains(id))
            for (const auto &member : tabGroups[id].members) out.insert(member);
    return out;
}
QStringList knownLimitsClients(const QStringList &present, const QStringList &quotaIds,
                              const QSet<QString> &placeholders)
{
    QStringList ids;
    for (const auto &id : present)
        for (const auto &member : tabSlice(id))
            if (placeholders.contains(member) || quotaIds.contains(member)) ids.append(member);
    return unique(ids + quotaIds);
}
QStringList orderedClients(const QStringList &ids, const QString &orderRaw)
{
    return ordered(ids, parseIdList(orderRaw));
}
QStringList orderedClients(const QStringList &ids, const ClientPreferences &p)
{
    return orderedClients(ids, p.order.value_or(QString()));
}
QSet<QString> hiddenTabClients(const QSet<QString> &raw)
{
    QSet<QString> tabs;
    for (const auto &id : raw) tabs.insert(tabId(id));
    return withGroupMembers(tabs);
}
QSet<QString> hiddenTabClients(const ClientPreferences &p) { return hiddenTabClients(hiddenClients(p)); }
QStringList displayClients(const QStringList &present, const QString &hiddenRaw, const QString &orderRaw)
{
    const auto hidden = hiddenTabClients(parseIdSet(hiddenRaw));
    auto visible = present;
    visible.removeIf([&](const auto &id) { return hidden.contains(id); });
    QStringList tabs;
    for (const auto &id : parseIdList(orderRaw)) tabs.append(tabId(id));
    return ordered(visible, unique(tabs));
}
QStringList displayClients(const QStringList &present, const ClientPreferences &p)
{
    return displayClients(present, p.hidden, p.order.value_or(QString()));
}
QStringList reorder(const QStringList &list, const QString &from, const QString &to)
{
    const auto fromI = list.indexOf(from), toI = list.indexOf(to);
    if (fromI < 0 || toI < 0 || fromI == toI) return list;
    auto out = list;
    out.removeAll(from);
    out.insert(out.indexOf(to) + (fromI < toI ? 1 : 0), from);
    return out;
}
QStringList mergeReorder(const QStringList &full, const QStringList &visible,
                        const QString &from, const QString &to)
{
    const auto queue = reorder(visible, from, to);
    QStringList out;
    qsizetype next = 0;
    for (const auto &id : full) {
        if (!visible.contains(id)) out.append(id);
        else if (next < queue.size()) out.append(queue[next++]);
    }
    while (next < queue.size()) out.append(queue[next++]);
    return out;
}
void migrateLegacyOrderKey(ClientPreferences &p)
{
    if (!p.order && p.legacyOrder && !p.legacyOrder->isEmpty()) p.order = p.legacyOrder;
}
}
