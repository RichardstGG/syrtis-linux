import Foundation
import TokenBarCore

private typealias ExtA = UsageAttribution
private typealias ExtS = UsageAttributionSettings

// Transport shipping values, adding only computed properties used by callers.
private func extJSON(_ value: Any) -> Any {
    let mirror = Mirror(reflecting: value)
    if mirror.displayStyle == .optional {
        return mirror.children.first.map { extJSON($0.value) } ?? NSNull()
    }
    if let state = value as? ExtA.State {
        switch state {
        case .assigned(let target): return ["kind": "assigned", "target": target]
        case .excluded: return ["kind": "excluded"]
        case .unassigned: return ["kind": "unassigned"]
        }
    }
    if value is String || value is Bool || value is Int || value is Int64 || value is Double { return value }
    if mirror.displayStyle == .collection { return mirror.children.map { extJSON($0.value) } }
    if mirror.displayStyle == .dictionary {
        var out: [String: Any] = [:]
        for child in mirror.children {
            let pair = Array(Mirror(reflecting: child.value).children)
            out[pair[0].value as! String] = extJSON(pair[1].value)
        }
        return out
    }
    var out = Dictionary(uniqueKeysWithValues: mirror.children.map { ($0.label!, extJSON($0.value)) })
    if let row = value as? ExtS.Row { out["id"] = row.id; out["providerLabel"] = row.providerLabel }
    if let row = value as? UsageAttributionBreakdown.Row { out["id"] = row.id }
    if let day = value as? SubscriptionTrend.Day { out["isEmpty"] = day.isEmpty }
    if let trend = value as? SubscriptionTrend {
        out["costOrder"] = trend.targets(byTokens: false)
        out["tokenOrder"] = trend.targets(byTokens: true)
    }
    return out
}

private func extError(_ error: Error) -> [String: Any] {
    func context(_ kind: String, _ c: DecodingError.Context, field: String? = nil) -> [String: Any] {
        var out: [String: Any] = ["kind": kind, "path": c.codingPath.map(\.stringValue)]
        if let field { out["field"] = field }
        return out
    }
    switch error {
    case DecodingError.keyNotFound(let key, let c): return context("keyNotFound", c, field: key.stringValue)
    case DecodingError.valueNotFound(_, let c): return context("valueNotFound", c)
    case DecodingError.typeMismatch(_, let c): return context("typeMismatch", c)
    case DecodingError.dataCorrupted(let c): return context("dataCorrupted", c)
    default: return ["kind": "unexpected"]
    }
}

private struct ExtRecord: Decodable {
    let client: String
    let provider: String
    let model: String?
    let state: String
    let target: String?
    var record: ExtA.Record {
        let s: ExtA.State
        switch state {
        case "assigned": s = .assigned(target!)
        case "excluded": s = .excluded
        default: s = .unassigned
        }
        return ExtA.Record(client: client, provider: provider, model: model, state: s)
    }
}

func attributionExt(_ fixtures: URL) throws -> [String: Any] {
    func invalid(_ message: String) -> NSError {
        NSError(domain: "AttributionExtHarness", code: 1, userInfo: [NSLocalizedDescriptionKey: message])
    }
    func decode<T: Decodable>(_ type: T.Type, _ value: Any) throws -> T {
        try JSONDecoder().decode(type, from: JSONSerialization.data(withJSONObject: value, options: [.fragmentsAllowed, .sortedKeys]))
    }
    let data = try Data(contentsOf: fixtures.appendingPathComponent("attribution-ext.json"))
    guard let root = try JSONSerialization.jsonObject(with: data) as? [String: Any],
          let cases = root["cases"] as? [[String: Any]] else { throw invalid("invalid attribution-ext cases") }
    var ids = Set<String>()
    var output: [[String: Any]] = []
    for c in cases {
        guard let id = c["id"] as? String, ids.insert(id).inserted,
              let op = c["op"] as? String,
              ["copy", "targets", "suggestion", "accept", "writeFailure", "daily", "window", "breakdown", "settings"].contains(op),
              let input = c["input"] as? [String: Any] else { throw invalid("invalid attribution-ext case") }
        func field<T: Decodable>(_ name: String, _ type: T.Type) throws -> T {
            guard let value = input[name] else { throw invalid("missing attribution-ext field") }
            return try decode(type, value)
        }
        func records(_ name: String) throws -> [ExtA.Record] {
            let raw = try field(name, [ExtRecord].self)
            guard raw.allSatisfy({ ["assigned", "excluded", "unassigned"].contains($0.state) && ($0.state != "assigned" || $0.target != nil) })
            else { throw invalid("invalid attribution-ext records") }
            return raw.map(\.record)
        }
        var result: [String: Any] = ["id": id]
        do {
            let value: [String: Any]
            switch op {
            case "copy":
                value = ["settings": ExtS.Copy.all, "breakdown": UsageAttributionBreakdown.Copy.all]
            case "targets":
                let payload = try field("payload", AgentUsagePayload?.self)
                value = ["subscriptions": ExtS.subscriptionClients(from: payload), "routed": ExtS.routedSubscriptions(from: payload),
                         "labels": try field("labels", [String].self).map { extJSON(ExtS.subscriptionClient(forLabel: $0) as Any) }]
            case "suggestion":
                value = ["state": extJSON(ExtS.suggestionTarget(sourceClient: try field("sourceClient", String.self),
                    provider: try field("provider", String.self), subscriptionClients: try field("subscriptions", [String].self),
                    routedSubscriptions: try field("routed", ExtS.RoutedSubscriptions.self)) as Any)]
            case "accept", "writeFailure":
                let updates = try records("records")
                // A fresh synthetic suite exercises the shipping storage wrapper;
                // it is removed even when either encoding or classification fails.
                let suite = "Syrtis.CrossCheck.AttributionExt." + UUID().uuidString
                guard let defaults = UserDefaults(suiteName: suite) else { throw invalid("cannot create synthetic defaults") }
                defer { defaults.removePersistentDomain(forName: suite) }
                if let raw = input["confirmedRaw"] { defaults.set(raw, forKey: ExtA.confirmedKey) }
                if let raw = input["suggestionsRaw"] { defaults.set(raw, forKey: ExtA.suggestionsKey) }
                if op == "accept" {
                    let failure = ExtS.accept(updates, defaults: defaults)
                    value = ["confirmedRaw": defaults.object(forKey: ExtA.confirmedKey) ?? NSNull(),
                             "suggestionsRaw": defaults.object(forKey: ExtA.suggestionsKey) ?? NSNull(), "failure": extJSON(failure as Any)]
                } else {
                    let encoded = ExtA.confirmedRaw(updating: defaults.object(forKey: ExtA.confirmedKey), records: updates)
                    value = ["result": extJSON(encoded as Any), "failure": extJSON(ExtS.writeFailure(
                        table: ExtA.confirmed(defaults: defaults), records: updates, result: encoded)?.message as Any)]
                }
            case "daily":
                let points = AttributedDailySeries.points(contributions: try field("contributions", [Contribution].self), confirmed: try records("confirmed"))
                let today = try field("today", String.self), days = try field("days", Int.self)
                value = ["points": extJSON(points), "trend": extJSON(SubscriptionTrendFold.build(points: points, today: today, days: days)),
                         "range": extJSON(SubscriptionTrendFold.calendarRange(endingAt: today, count: days) as Any)]
            case "window":
                let usage = try field("usage", WindowUsage.self)
                let totals = usage.totals(confirmed: try records("confirmed"))
                value = ["undatedCount": usage.undatedCount, "processingTimeMs": usage.processingTimeMs,
                         "messages": usage.messages.map { ["tokens": $0.tokens, "tokensExCacheRead": $0.tokensExCacheRead] },
                         "assigned": extJSON(totals.assigned), "excluded": extJSON(totals.excluded), "unassigned": extJSON(totals.unassigned),
                         "buckets": usage.buckets(from: try field("from", Int64.self), bucketMs: try field("bucketMs", Int64.self), count: try field("count", Int.self))]
            case "breakdown":
                value = ["rows": extJSON(UsageAttributionBreakdown.rows(entries: try field("entries", [ModelReportEntry].self),
                    clientIds: try field("clients", [String].self), confirmed: try records("confirmed")))]
            case "settings":
                let entries = try field("entries", [ModelReportEntry].self)
                let confirmed = try records("confirmed"), suggestions = try records("suggestions")
                let subscriptions = try field("subscriptions", [String].self)
                let routed = try field("routed", ExtS.RoutedSubscriptions.self)
                let rows = ExtS.rows(entries: entries, confirmed: confirmed, suggestions: suggestions)
                let page = ExtS.pageState(hasReport: try field("hasReport", Bool.self), rowCount: rows.count, isLoading: try field("isLoading", Bool.self))
                let pageName: String
                switch page {
                case .loading: pageName = "loading"
                case .unavailable: pageName = "unavailable"
                case .empty: pageName = "empty"
                case .rows: pageName = "rows"
                }
                value = ["rows": extJSON(rows), "proposed": extJSON(ExtS.suggestionRecords(entries: entries, confirmed: confirmed,
                    subscriptionClients: subscriptions, routedSubscriptions: routed)), "accepted": extJSON(ExtS.acceptanceRecords(rows: rows)),
                    "onboarding": extJSON(ExtS.onboardingSummary(entries: entries, confirmed: confirmed, subscriptionClients: subscriptions, routedSubscriptions: routed)),
                    "pageState": pageName, "signature": ExtS.signature(entries: entries, subscriptionClients: subscriptions,
                        targetsKnown: try field("targetsKnown", Bool.self), routedSubscriptions: routed)]
            default: throw invalid("unknown attribution-ext operation")
            }
            result["value"] = value
        } catch let error as NSError where error.domain == "AttributionExtHarness" { throw error }
        catch { result["error"] = extError(error) }
        output.append(result)
    }
    return ["cases": output]
}
