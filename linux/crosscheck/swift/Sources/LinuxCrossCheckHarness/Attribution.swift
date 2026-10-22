import Foundation
import TokenBarCore

private typealias A = UsageAttribution
private typealias H = QuotaHistoryFold
private typealias E = WindowEquivalence

// Reflection transports shipping values; computed properties are added explicitly.
private func attributionJSON(_ value: Any) -> Any {
    let mirror = Mirror(reflecting: value)
    if mirror.displayStyle == .optional {
        return mirror.children.first.map { attributionJSON($0.value) } ?? NSNull()
    }
    if let state = value as? A.State {
        switch state {
        case .assigned(let target): return ["kind": "assigned", "target": target]
        case .excluded: return ["kind": "excluded"]
        case .unassigned: return ["kind": "unassigned"]
        }
    }
    if value is String || value is Bool || value is Int || value is Int64 || value is Double { return value }
    if mirror.displayStyle == .collection { return mirror.children.map { attributionJSON($0.value) } }
    var out = Dictionary(uniqueKeysWithValues: mirror.children.map { ($0.label!, attributionJSON($0.value)) })
    if let b = value as? TokenBreakdown { out["total"] = b.total }
    if let c = value as? QuotaCycle {
        out["evidenceStartMs"] = c.evidenceStartMs
        out["durationMs"] = c.durationMs
    }
    if let m = value as? QuotaHistoryModel { out["id"] = m.id }
    if let r = value as? QuotaHistoryRow { out["id"] = r.id }
    if let z = value as? HitZone {
        out["consumedUsed"] = attributionJSON(z.consumed(.used) as Any)
        out["consumedRemaining"] = attributionJSON(z.consumed(.remaining) as Any)
    }
    return out
}

private func attributionRow(_ row: E.Row) -> [String: Any] {
    var out: [String: Any] = ["isRatio": row.isRatio]
    switch row {
    case .ratio(let tokens, let cost, let error):
        out.merge(["kind": "ratio", "tokensPerTenth": tokens, "costPerTenth": cost, "errorPercent": error]) { _, new in new }
    case .insufficient(let delta, let error):
        out.merge(["kind": "insufficient", "deltaPercent": delta, "errorPercent": error]) { _, new in new }
    case .notMoved: out["kind"] = "notMoved"
    case .unaccounted(let delta): out.merge(["kind": "unaccounted", "deltaPercent": delta]) { _, new in new }
    case .unavailable: out["kind"] = "unavailable"
    case .costOnly(let cost, let error): out.merge(["kind": "costOnly", "costPerTenth": cost, "errorPercent": error]) { _, new in new }
    case .tokensOnly(let tokens, let error): out.merge(["kind": "tokensOnly", "tokensPerTenth": tokens, "errorPercent": error]) { _, new in new }
    case .tooFewCycles(let count, let needed): out.merge(["kind": "tooFewCycles", "count": count, "needed": needed]) { _, new in new }
    case .undeclared: out["kind"] = "undeclared"
    case .spread(let low, let high, let lowCost, let highCost):
        out.merge(["kind": "spread", "lowPerTenth": low, "highPerTenth": high, "lowCostPerTenth": lowCost, "highCostPerTenth": highCost]) { _, new in new }
    }
    out["text"] = E.text(row, tokens: { String($0) }, money: { String(format: "%.3f", $0) })
    return out
}

private func attributionError(_ error: Error) -> [String: Any] {
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

private struct AttributionSample: Decodable {
    let atMs: Int64
    let usedPercent: Double
    var sample: QuotaSample { QuotaSample(atMs: atMs, usedPercent: usedPercent) }
}
private struct AttributionCycle: Decodable {
    let deltaPercent: Double
    let spanTokens: Int64
    let spanCost: Double
    let observedFraction: Double
    let risingRuns: Int
    var cycle: E.Cycle {
        E.Cycle(deltaPercent: deltaPercent, spanTokens: spanTokens, spanCost: spanCost,
                observedFraction: observedFraction, risingRuns: risingRuns)
    }
}

func attribution(_ fixtures: URL) throws -> [String: Any] {
    func invalid(_ message: String) -> NSError {
        NSError(domain: "AttributionHarness", code: 1, userInfo: [NSLocalizedDescriptionKey: message])
    }
    func decode<T: Decodable>(_ type: T.Type, _ value: Any) throws -> T {
        try JSONDecoder().decode(type, from: JSONSerialization.data(withJSONObject: value, options: [.fragmentsAllowed, .sortedKeys]))
    }
    func records(_ value: Any?) throws -> [A.Record] {
        guard let array = value as? [[String: Any]] else { throw invalid("invalid attribution records") }
        return try array.map { r in
            guard let client = r["client"] as? String, let provider = r["provider"] as? String,
                  let state = r["state"] as? String else { throw invalid("invalid attribution record") }
            let s: A.State
            switch state {
            case "assigned":
                guard let target = r["target"] as? String else { throw invalid("invalid attribution target") }
                s = .assigned(target)
            case "excluded": s = .excluded
            case "unassigned": s = .unassigned
            default: throw invalid("invalid attribution state")
            }
            return A.Record(client: client, provider: provider, model: r["model"] as? String, state: s)
        }
    }
    let data = try Data(contentsOf: fixtures.appendingPathComponent("attribution.json"))
    guard let root = try JSONSerialization.jsonObject(with: data) as? [String: Any],
          let cases = root["cases"] as? [[String: Any]] else { throw invalid("invalid attribution cases") }
    var output: [[String: Any]] = []
    var ids = Set<String>()
    for c in cases {
        guard let id = c["id"] as? String, ids.insert(id).inserted,
              let op = c["op"] as? String, ["codec", "aggregate", "readings", "row", "geometry", "history"].contains(op),
              let input = c["input"] as? [String: Any] else { throw invalid("invalid attribution case") }
        func field<T: Decodable>(_ name: String, _ type: T.Type) throws -> T {
            guard let value = input[name] else { throw invalid("missing attribution field") }
            return try decode(type, value)
        }
        func samples() throws -> [QuotaSample] { try field("samples", [AttributionSample].self).map(\.sample) }
        var result: [String: Any] = ["id": id]
        do {
            let value: [String: Any]
            switch op {
            case "codec":
                let raw = input["raw"]
                let table = A.parseRaw(raw as? String)
                let updates = try records(input["records"])
                // The public replacement API supplies the foreign-value guard
                // without exposing private parseState or touching UserDefaults.
                let writable = A.suggestionsRaw(replacing: raw, with: []) != nil
                guard let queries = input["queries"] as? [[String: Any]] else { throw invalid("invalid attribution queries") }
                let states = try queries.map { q -> Any in
                    guard let client = q["client"] as? String, let provider = q["provider"] as? String else { throw invalid("invalid attribution query") }
                    return attributionJSON(A.resolve(client: client, provider: provider, model: q["model"] as? String, records: table.records))
                }
                value = ["records": attributionJSON(table.records), "isWritable": writable, "states": states,
                         "declares": A.declares(subscription: try field("subscription", String.self), records: table.records),
                         "updatedRaw": attributionJSON(A.confirmedRaw(updating: raw, records: updates) as Any),
                         "suggestedRaw": attributionJSON(A.suggestionsRaw(updating: raw, records: updates) as Any),
                         "replacedRaw": attributionJSON(A.suggestionsRaw(replacing: raw, with: updates) as Any)]
            case "aggregate":
                value = ["row": attributionRow(E.aggregate(declared: try field("declared", Bool.self), cycles: try field("cycles", [AttributionCycle].self).map(\.cycle)))]
            case "readings":
                let readings = try field("readings", [Double].self)
                value = ["consumed": H.consumed(readings), "risingRuns": H.risingRuns(readings),
                         "deltaQualifies": E.deltaQualifies(H.consumed(readings), runs: H.risingRuns(readings)),
                         "lowerBound": H.lowerBound(try field("stamps", [Int64].self), try field("probe", Int64.self))]
            case "row":
                value = ["row": attributionRow(E.row(samples: try samples(), messages: try field("messages", [WindowMessage].self)))]
            case "geometry":
                guard let metric = QuotaMetric(rawValue: try field("metric", String.self)) else { throw invalid("invalid attribution metric") }
                let geometry = WindowCardGeometry.chartGeometry(
                    windowStartMs: try field("start", Int64.self), windowEndMs: try field("end", Int64.self), nowMs: try field("now", Int64.self),
                    samples: try samples(), messages: try field("messages", [WindowMessage].self), metric: metric)
                value = attributionJSON(geometry) as! [String: Any]
            case "history":
                let cycles = H.cycles(points: try field("points", [QuotaCurvePoint].self))
                let considered = H.considered(cycles)
                let messages = try field("messages", [WindowMessage].self)
                let subscription = try field("subscription", String.self)
                let scope = input["modelScope"] as? String
                let confirmed = try records(input["records"])
                let rows = H.rows(cycles: considered, messages: messages, subscription: subscription, modelScope: scope, confirmed: confirmed)
                let spans = H.spans(cycles: considered, messages: messages, subscription: subscription, modelScope: scope, confirmed: confirmed)
                value = ["cycles": attributionJSON(cycles), "considered": attributionJSON(considered), "rows": attributionJSON(rows),
                         "spans": spans.map { ["tokens": $0.tokens, "cost": $0.cost] as [String: Any] },
                         "aggregate": attributionRow(E.aggregate(subscription: subscription, records: confirmed, cycles: rows.map {
                             E.Cycle(deltaPercent: $0.cycle.usedPercent, spanTokens: $0.spanTokens, spanCost: $0.spanCost,
                                     observedFraction: $0.cycle.observedFraction, risingRuns: $0.cycle.risingRuns)
                         }))]
            default: throw invalid("unknown attribution operation")
            }
            result["value"] = value
        } catch let error as NSError where error.domain == "AttributionHarness" { throw error }
        catch { result["error"] = attributionError(error) }
        output.append(result)
    }
    return ["cases": output]
}
