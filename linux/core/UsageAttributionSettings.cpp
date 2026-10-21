#include "UsageAttributionSettings.h"
#include "AttributionString.h"
#include "ClientRegistry.h"
#include "TokenBreakdown.h"
#include <algorithm>
#include <bit>

namespace syrtis::core::UsageAttributionSettings {
namespace A = UsageAttribution;
namespace Copy {
const QString section = "Usage attribution";
const QString classifyHint = "Pick which subscription each source's usage counts toward. This only changes how Syrtis groups your usage; it doesn't read your bills.";
const QString canonicalizationHint = "Providers are matched exactly as each tool reports them, so similar ones can appear on separate rows. Some tools merge providers before reporting (Vertex AI shows up as Anthropic, Codex as OpenAI), and those rows can't be split.";
const QString noRows = "No usage in this range lists a provider.";
const QString unavailable = "Usage couldn't be loaded.";
const QString acceptSuggestions = "Accept all suggestions (%lld)";
const QString suggestionsHint = "Suggestions apply only when you accept them.";
const QString source = "%@ · %@";
const QString observed = "Observed %@ tokens · %@";
const QString classification = "Classification";
const QString unassigned = "Unassigned";
const QString excluded = "Not a subscription";
const QString assigned = "Counts toward %@";
const QString suggested = "Suggested: counts toward %@";
const QString suggestedExcluded = "Suggested: not a subscription";
const QString unspecifiedProvider = "Unspecified provider";
const QString classificationFor = "Classification for %@";
QStringList all() { return {section, classifyHint, canonicalizationHint, noRows, unavailable, acceptSuggestions, suggestionsHint, source, observed, classification, unassigned, excluded, assigned, suggested, suggestedExcluded, unspecifiedProvider, classificationFor, message(WriteFailure::invalidExistingValue), message(WriteFailure::entryLimit), message(WriteFailure::sizeOrInvalidRecord)}; }
}
const QSet<QString> crossAgentSubscriptionProviders = {"openai", "xai", "deepseek", "moonshot", "minimax", "zhipu", "alibaba", "open-weights", "own", "microsoft", "amazon"};
const QSet<QString> subscriptionBoundProviders = {"anthropic", "google"};
const QSet<QString> ownRowsOnlySubscriptions = {"grok-bot"};
const QMap<QString, QString> providerOwnClient = {
    {"anthropic", "claude"},
    {"openai", "codex"},
    {"google", "antigravity"},
    {"xai", "grok"},
    {"moonshot", "kimi"},
    {"minimax", "micode"},
    {"alibaba", "qwen"},
};
const QMap<QString, QSet<QString>> subscriptionProviderMap = {
    {"claude", {"anthropic"}},
    {"codex", {"openai"}},
    {"grok", {"xai"}},
    {"grok-bot", {"xai"}},
    {"kimi", {"moonshot"}},
    {"micode", {"minimax"}},
    {"copilot", {"openai", "anthropic", "google", "xai", "microsoft", "moonshot"}},
    {"antigravity", {"google", "anthropic", "open-weights"}},
    {"cursor", {"anthropic", "openai", "google", "xai", "own", "moonshot", "zhipu"}},
    {"zed", {"anthropic", "openai", "google"}},
    {"junie", {"openai", "anthropic", "google", "xai", "amazon"}},
    {"trae", {"openai", "anthropic", "minimax"}},
    {"amp", {"openai", "anthropic", "zhipu", "open-weights"}},
    {"droid", {"anthropic", "openai", "google", "moonshot", "zhipu", "open-weights"}},
    {"kiro", {"anthropic", "open-weights", "alibaba", "deepseek", "minimax"}},
    {"warp", {"openai", "anthropic", "google", "xai", "open-weights", "zhipu", "moonshot", "minimax", "alibaba", "deepseek"}},
    {"kilo", {"anthropic", "openai", "google", "xai", "deepseek", "moonshot", "minimax", "zhipu", "alibaba", "open-weights"}},
    {"kilocode", {"anthropic", "openai", "google", "xai", "deepseek", "moonshot", "minimax", "zhipu", "alibaba", "open-weights"}},
    {"cline", {"zhipu", "moonshot", "deepseek", "minimax", "alibaba", "open-weights"}},
    {"qwen", {"alibaba", "zhipu", "moonshot", "minimax"}},
};
QString message(WriteFailure failure) {
    switch (failure) {
    case WriteFailure::invalidExistingValue: return "Could not save this classification: existing attribution data is invalid or foreign.";
    case WriteFailure::entryLimit: return "Could not save this classification: the attribution entry limit was reached.";
    case WriteFailure::sizeOrInvalidRecord: return "Could not save this classification: the new value is too large or unsupported.";
    }
    return {};
}
bool Row::operator==(const Row &other) const {
    return attributionKey(client) == attributionKey(other.client) && attributionKey(provider) == attributionKey(other.provider)
        && tokens == other.tokens && cost == other.cost && state == other.state && suggestedState == other.suggestedState;
}
QString Row::id() const { return client + QChar(0) + provider; }
QString Row::providerLabel() const { return provider.isEmpty() ? Copy::unspecifiedProvider : provider; }
std::optional<QString> subscriptionClient(const QString &label) {
    if (ClientRegistry::subscriptionLabelAliases.contains(label)) return ClientRegistry::subscriptionLabelAliases.value(label);
    const auto vendor = label.toLower().section('-', 0, 0);
    if (providerOwnClient.contains(vendor)) return providerOwnClient.value(vendor);
    return subscriptionProviderMap.contains(vendor) ? std::optional<QString>(vendor) : std::nullopt;
}
QStringList subscriptionClients(const std::optional<Syrtis::AgentUsagePayload> &payload) {
    QStringList candidates, result;
    if (!payload) return result;
    for (const auto &s : payload->agents)
        if ((s.identity || !s.windows.isEmpty() || s.credits) && s.clientId != "opencode") candidates.append(s.clientId);
    for (const auto &label : payload->opencodeSubscriptions.value_or(QStringList{}))
        if (const auto client = subscriptionClient(label)) candidates.append(*client);
    for (const auto &id : candidates)
        if (ClientRegistry::allIds().contains(id) && !result.contains(id)) result.append(id);
    return result;
}
RoutedSubscriptions routedSubscriptions(const std::optional<Syrtis::AgentUsagePayload> &payload) {
    QStringList clients;
    if (payload) for (const auto &label : payload->opencodeSubscriptions.value_or(QStringList{}))
        if (const auto client = subscriptionClient(label); client && ClientRegistry::allIds().contains(*client)) clients.append(*client);
    return clients.isEmpty() ? RoutedSubscriptions{} : RoutedSubscriptions{{"opencode", clients}};
}
PageState pageState(bool hasReport, qsizetype rowCount, bool isLoading) {
    return !hasReport ? (isLoading ? PageState::loading : PageState::unavailable) : rowCount == 0 ? PageState::empty : PageState::rows;
}
QList<Row> rows(const QList<ModelReportEntry> &entries, const QList<A::Record> &confirmed, const QList<A::Record> &suggestions) {
    QList<Row> out;
    QMap<QString, qsizetype> indexes;
    for (const auto &e : entries) {
        if (e.total == 0 && e.cost == 0) continue;
        const auto key = attributionKey(e.client + QChar(0) + e.provider);
        if (indexes.contains(key)) {
            auto &r = out[indexes[key]];
            r.tokens = saturatingAdding(r.tokens, e.total);
            r.cost += e.cost;
        } else {
            indexes[key] = out.size();
            const auto state = A::resolve(e.client, e.provider, std::nullopt, confirmed);
            std::optional<A::State> suggestion;
            if (state.kind == A::State::Kind::unassigned) for (const auto &r : suggestions)
                if (!r.model && attributionKey(r.client) == attributionKey(e.client) && attributionKey(r.provider) == attributionKey(e.provider)) {
                    suggestion = r.state; break;
                }
            out.append({e.client, e.provider, e.total, e.cost, state, suggestion});
        }
    }
    return out;
}
std::optional<A::State> suggestionTarget(const QString &sourceClient, const QString &provider,
    const QStringList &subscriptions, const RoutedSubscriptions &routed) {
    const auto sourceOwner = ClientRegistry::quotaOwner(sourceClient);
    if (subscriptionProviderMap.value(sourceOwner).contains(provider)) return A::State{A::State::Kind::assigned, sourceOwner};
    if (routed.contains(sourceOwner)) {
        QStringList covering;
        for (const auto &client : routed[sourceOwner]) if (subscriptionProviderMap.value(client).contains(provider)) covering.append(client);
        return covering.size() == 1 ? std::optional<A::State>({A::State::Kind::assigned, covering[0]}) : std::nullopt;
    }
    if (!subscriptionProviderMap.contains(sourceOwner)) return std::nullopt;
    QStringList eligible;
    const bool bound = subscriptionBoundProviders.contains(provider);
    for (const auto &client : subscriptions) {
        if (ownRowsOnlySubscriptions.contains(client) || !subscriptionProviderMap.value(client).contains(provider)) continue;
        if (bound ? client != providerOwnClient.value(provider) : crossAgentSubscriptionProviders.contains(provider)) eligible.append(client);
    }
    if (eligible.size() == 1) return A::State{A::State::Kind::assigned, eligible[0]};
    return eligible.isEmpty() && bound ? std::optional<A::State>({A::State::Kind::excluded, {}}) : std::nullopt;
}
QList<A::Record> suggestionRecords(const QList<ModelReportEntry> &entries, const QList<A::Record> &confirmed,
    const QStringList &subscriptions, const RoutedSubscriptions &routed) {
    QList<A::Record> out;
    for (const auto &row : rows(entries, confirmed, {})) {
        if (row.state.kind != A::State::Kind::unassigned) continue;
        if (const auto proposed = suggestionTarget(row.client, row.provider, subscriptions, routed))
            out.append({row.client, row.provider, std::nullopt, *proposed});
    }
    return out;
}
QList<A::Record> acceptanceRecords(const QList<Row> &rows) {
    QList<A::Record> out;
    for (const auto &r : rows) if (r.state.kind == A::State::Kind::unassigned && r.suggestedState)
        out.append({r.client, r.provider, std::nullopt, *r.suggestedState});
    return out;
}
OnboardingSummary onboardingSummary(const QList<ModelReportEntry> &entries, const QList<A::Record> &confirmed,
    const QStringList &subscriptions, const RoutedSubscriptions &routed) {
    const auto allRows = rows(entries, confirmed, suggestionRecords(entries, confirmed, subscriptions, routed));
    const auto records = acceptanceRecords(allRows);
    return {records, std::count_if(allRows.begin(), allRows.end(), [](const auto &r) { return r.state.kind == A::State::Kind::unassigned; }) - records.size()};
}
std::optional<WriteFailure> writeFailure(const A::Table &table, const QList<A::Record> &updates, const std::optional<QString> &result) {
    if (result) return std::nullopt;
    if (!table.isWritable) return WriteFailure::invalidExistingValue;
    auto records = table.records;
    for (const auto &update : updates) {
        records.removeIf([&](const auto &r) {
            auto a = r, b = update;
            a.state = {}; b.state = {};
            return a == b;
        });
        if (update.state.kind != A::State::Kind::unassigned) records.append(update);
    }
    return records.size() > A::maxEntries ? WriteFailure::entryLimit : WriteFailure::sizeOrInvalidRecord;
}
std::optional<WriteFailure> writeFailure(const A::Table &table, const A::Record &record, const std::optional<QString> &result) {
    return writeFailure(table, QList<A::Record>{record}, result);
}
Acceptance accept(const QList<A::Record> &records, const QJsonValue &confirmed, const QJsonValue &suggestions) {
    if (records.isEmpty()) return {confirmed, suggestions, std::nullopt};
    const auto confirmedRaw = A::confirmedRaw(confirmed, records);
    if (!confirmedRaw) return {confirmed, suggestions, message(*writeFailure(A::parseState(confirmed), records, confirmedRaw))};
    auto removals = records;
    for (auto &r : removals) r.state = {};
    const auto suggestionsRaw = A::suggestionsRaw(suggestions, removals);
    if (!suggestionsRaw) return {confirmed, suggestions, message(*writeFailure(A::parseState(suggestions), removals, suggestionsRaw))};
    return {*confirmedRaw, *suggestionsRaw, std::nullopt};
}
QString signature(const QList<ModelReportEntry> &entries, const QStringList &subscriptions, bool targetsKnown, const RoutedSubscriptions &routed) {
    QStringList entryParts, routes;
    for (const auto &e : entries) entryParts.append(QStringList{e.client, e.provider, e.model, QString::number(e.total),
        QString::number(std::bit_cast<quint64>(e.cost))}.join(QChar(0x1f)));
    auto keys = routed.keys();
    std::sort(keys.begin(), keys.end(), attributionLess);
    for (const auto &key : keys) {
        auto clients = routed[key];
        std::sort(clients.begin(), clients.end(), attributionLess);
        routes.append(key + ">" + clients.join('+'));
    }
    return entryParts.join(QChar(0x1e)) + "|" + (targetsKnown ? "known" : "pending") + "|" + subscriptions.join(',') + "|" + routes.join(';');
}
}
