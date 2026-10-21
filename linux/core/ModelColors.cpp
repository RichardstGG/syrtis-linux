#include "ModelColors.h"
#include "ModelGrouping.h"
#include <algorithm>
#include <cmath>

namespace syrtis::core {
const QHash<QString, QString> ModelColors::providerBase{
    {"anthropic", "#da7756"}, {"openai", "#3b82f6"}, {"google", "#06b6d4"},
    {"cursor", "#a855f7"}, {"deepseek", "#4d6bfe"}, {"xai", "#1f2937"},
    {"meta", "#0866ff"}, {"mistral", "#fa520f"}, {"unknown", "#888888"}};
QString ModelColors::providerFromModel(const QString &model)
{
    const auto m = model.toLower();
    if (m.contains("claude") || m.contains("sonnet") || m.contains("opus") || m.contains("haiku")) return "anthropic";
    if (m.contains("gpt") || m.startsWith("o1") || m.startsWith("o3") || m.contains("codex")
        || m.contains("text-embedding") || m.contains("dall-e") || m.contains("whisper") || m.contains("tts")) return "openai";
    if (m.contains("gemini")) return "google";
    if (m.contains("deepseek")) return "deepseek";
    if (m.contains("grok")) return "xai";
    if (m.contains("llama")) return "meta";
    if (m.contains("mixtral") || m.contains("mistral")) return "mistral";
    if (m == "auto" || m.contains("cursor") || m.contains("composer")) return "cursor";
    return "unknown";
}
QString ModelColors::providerColorKey(const std::optional<QString> &providerId, const QString &modelId)
{
    const auto p = providerId.value_or("").toLower();
    if (p.isEmpty() || p.contains(", ")) return providerFromModel(modelId);
    if (p.contains("anthropic")) return "anthropic";
    if (p.contains("openai")) return "openai";
    if (p.contains("google") || p.contains("gemini")) return "google";
    if (p.contains("deepseek")) return "deepseek";
    if (p.contains("xai") || p.contains("grok")) return "xai";
    if (p.contains("meta") || p.contains("llama")) return "meta";
    if (p.contains("mistral")) return "mistral";
    if (p.contains("cursor")) return "cursor";
    return providerBase.contains(p) ? p : providerFromModel(modelId);
}
QString ModelColors::shadeFromBase(const QString &hex, qint64 rank)
{
    const auto base = hex.startsWith('#') ? hex.mid(1) : hex;
    if (base.size() != 6) return hex;
    int channels[3];
    for (int i = 0; i < 3; ++i) {
        const auto part = base.mid(i * 2, 2);
        // Swift Int(radix:16) accepts signs, but never trims whitespace.
        bool ok = false;
        channels[i] = part.toInt(&ok, 16);
        if (!ok || part != part.trimmed()) return hex;
    }
    constexpr double factors[]{0.0, 0.11, 0.22, 0.33, 0.44, 0.56, 0.67};
    const auto f = factors[std::clamp<qint64>(rank, 0, 6)];
    QString result = "#";
    for (const int c : channels)
        result += QString::asprintf("%02x", int(std::round(double(c) + (255.0 - double(c)) * f)));
    return result;
}
ModelColorMap::ModelColorMap(const QList<ModelColorEntry> &entries)
{
    QHash<QString, QHash<QString, double>> byProvider;
    for (const auto &e : entries)
        byProvider[ModelColors::providerColorKey(e.provider, e.model)][ModelGrouping::groupID(e.model)]
            += std::isfinite(e.cost) ? e.cost : 0;
    for (auto it = byProvider.begin(); it != byProvider.end(); ++it) {
        auto models = it->keys();
        std::sort(models.begin(), models.end(), [&](const auto &a, const auto &b) {
            return it->value(a) != it->value(b) ? it->value(a) > it->value(b) : a < b;
        });
        const auto base = ModelColors::providerBase.value(it.key(), ModelColors::providerBase["unknown"]);
        for (qsizetype rank = 0; rank < models.size(); ++rank)
            colorByKeyModel[it.key() + ' ' + models[rank]] = ModelColors::shadeFromBase(base, rank);
    }
}
ModelColorMap::ModelColorMap(const std::optional<ModelReport> &report)
    : ModelColorMap([&] {
        QList<ModelColorEntry> entries;
        if (report) for (const auto &e : report->entries) entries.append({e.provider, e.model, e.cost});
        return entries;
    }()) {}
QString ModelColorMap::color(const std::optional<QString> &providerId, const QString &modelId) const
{
    const auto key = ModelColors::providerColorKey(providerId, modelId);
    const auto lookup = key + ' ' + ModelGrouping::groupID(modelId);
    if (colorByKeyModel.contains(lookup)) return colorByKeyModel[lookup];
    return ModelColors::shadeFromBase(ModelColors::providerBase.value(key, ModelColors::providerBase["unknown"]), 0);
}
}
