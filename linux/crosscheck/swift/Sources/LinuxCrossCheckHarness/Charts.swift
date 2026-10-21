import Foundation
import TokenBarCore

// Serialize shipping values, including optional nulls. Computed properties are
// added explicitly; the oracle never duplicates model logic or wire decoders.
private func chartJSON(_ value: Any) -> Any {
    let mirror = Mirror(reflecting: value)
    if mirror.displayStyle == .optional {
        return mirror.children.first.map { chartJSON($0.value) } ?? NSNull()
    }
    if value is String || value is Bool || value is Int || value is Int64 || value is Double { return value }
    switch mirror.displayStyle {
    case .collection: return mirror.children.map { chartJSON($0.value) }
    case .dictionary:
        var out: [String: Any] = [:]
        for item in mirror.children {
            let pair = Array(Mirror(reflecting: item.value).children)
            out[pair[0].value as! String] = chartJSON(pair[1].value)
        }
        return out
    case .struct, .tuple:
        var out = Dictionary(uniqueKeysWithValues: mirror.children.map { ($0.label!, chartJSON($0.value)) })
        if let day = value as? PerDay { out["isActive"] = day.isActive }
        if let bar = value as? DayBar {
            out["totalTokens"] = bar.totalTokens
            out["totalCost"] = bar.totalCost
            out["isEmpty"] = bar.isEmpty
        }
        return out
    default: return value
    }
}

private func chartError(_ error: Error) -> [String: Any] {
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

private func chartBytes(_ value: Any) throws -> Data {
    try JSONSerialization.data(withJSONObject: value, options: [.fragmentsAllowed, .sortedKeys])
}
private func chartDecode<T: Decodable>(_ type: T.Type, _ value: Any) throws -> T {
    try JSONDecoder().decode(type, from: chartBytes(value))
}
private func chartEntries(_ c: [String: Any]) -> [(provider: String, model: String, cost: Double)] {
    (c["entries"] as? [[String: Any]] ?? []).map { e in
        let cost: Double
        switch e["cost"] as? String {
        case "nan": cost = .nan
        case "infinity": cost = .infinity
        default: cost = (e["cost"] as! NSNumber).doubleValue
        }
        return (e["provider"] as! String, e["model"] as! String, cost)
    }
}
private func chartLegend(_ bars: [DayBar], _ metric: ChartMetric) -> Any {
    // Swift's Dictionary makes equal-weight legend order unspecified. Normalize
    // only ties for the oracle, without changing shipping ordering semantics.
    var rows = DayBars.legend(bars: bars, metric: metric)
    var start = 0
    while start < rows.count {
        var end = start + 1
        while end < rows.count && (metric == .cost ? rows[end].cost == rows[start].cost : rows[end].tokens == rows[start].tokens) {
            end += 1
        }
        rows.replaceSubrange(start..<end, with: rows[start..<end].sorted { $0.key < $1.key })
        start = end
    }
    return chartJSON(rows)
}
private func chartDecoded<T: Decodable>(_ type: T.Type, _ c: [String: Any]) throws -> [String: Any] {
    let bytes: Data
    if let raw = c["rawJSON"] as? String { bytes = Data(raw.utf8) }
    else { bytes = try chartBytes(c["input"]!) }
    let value = try JSONDecoder().decode(type, from: bytes)
    var out = ["value": chartJSON(value)]
    if let tokens = value as? TokenBreakdown { out["total"] = tokens.total }
    return out
}
private func chartCase(_ c: [String: Any]) throws -> [String: Any] {
    switch c["op"] as! String {
    case "payload":
        let payload = try chartDecode(UsagePayload.self, c["input"]!)
        let selected = c["selected"] as! [String], hidden = Set(c["hidden"] as! [String])
        let only = (c["only"] as? [String]).map { Set($0) }
        let stats = UsageStats(payload: payload, selectedClients: Set(selected))
        let colors = ModelColorMap(entries: chartEntries(c))
        var out: [String: Any] = [
            "value": chartJSON(payload), "stats": chartJSON(stats),
            "tray": chartJSON(payload.trayTotals(hidden: hidden, today: c["today"] as! String, only: only)),
            "visibleYears": UsageStats.yearsWithVisibleActivity(contributions: payload.contributions, hidden: hidden).sorted(),
            "hasVisibleActivity": UsageStats.hasVisibleActivity(contributions: payload.contributions, hidden: hidden),
            "turns": payload.contributions.map { ["selected": chartJSON($0.turns(for: selected) as Any),
                                                "empty": chartJSON($0.turns(for: []) as Any)] },
        ]
        for mode in StackBy.allCases {
            let bars = DayBars.build(payload: payload, clientIds: selected, stackBy: mode, colors: colors,
                                     rangeEnd: c["rangeEnd"] as! String, endFallback: c["endFallback"] as! String)
            out[mode.rawValue] = ["bars": chartJSON(bars), "tokenLegend": chartLegend(bars, .tokens),
                                  "costLegend": chartLegend(bars, .cost)]
        }
        return out
    case "colors":
        let colors = ModelColorMap(entries: chartEntries(c))
        let report: ModelReport?
        if let input = c["report"], !(input is NSNull) { report = try chartDecode(ModelReport.self, input) }
        else { report = nil }
        let reportColors = ModelColorMap(report: report)
        let lookups = (c["lookups"] as! [[String: Any]]).map { lookup -> [String: Any] in
            let provider = lookup["provider"] as? String, model = lookup["model"] as! String
            return ["inferred": ModelColors.providerFromModel(model), "key": ModelColors.providerColorKey(provider, model),
                    "color": colors.color(provider, model), "reportColor": reportColors.color(provider, model)]
        }
        let shades = (c["shades"] as! [[String: Any]]).map {
            ModelColors.shadeFromBase($0["hex"] as! String, rank: ($0["rank"] as! NSNumber).intValue)
        }
        return ["providerBase": ModelColors.providerBase, "lookups": lookups, "shades": shades]
    case "trace":
        let buckets = try chartDecode([TraceBucket].self, c["input"]!)
        return ["value": chartJSON(buckets), "totalRate": TraceBucket.totalRate(buckets, hidden: Set(c["hidden"] as! [String])),
                "collapsed": chartJSON(TraceBucket.collapseByClient(buckets))]
    case "overview":
        return ["allCases": OverviewCard.allCases.map(\.rawValue), "toggleable": OverviewCard.toggleable.map(\.rawValue),
                "hiddenKey": OverviewCard.hiddenKey,
                "labels": Dictionary(uniqueKeysWithValues: OverviewCard.allCases.map { ($0.rawValue, $0.label) }),
                "visible": OverviewCard.visible(hiddenRaw: c["hiddenRaw"] as! String).map(\.rawValue)]
    case "streaks":
        var days: [String: PerDay] = [:]
        for day in c["days"] as! [[String: Any]] {
            let date = day["date"] as! String
            days[date] = PerDay(date: date, tokens: (day["tokens"] as! NSNumber).int64Value,
                               cost: (day["cost"] as! NSNumber).doubleValue, intensity: 0,
                               hasMessages: day["hasMessages"] as! Bool)
        }
        return chartJSON(Streaks.compute(perDayMap: days, rangeStart: c["start"] as! String,
                                         rangeEnd: c["end"] as! String)) as! [String: Any]
    case "decode":
        switch c["type"] as! String {
        case "TokenBreakdown": return try chartDecoded(TokenBreakdown.self, c)
        case "ContributionClient": return try chartDecoded(ContributionClient.self, c)
        case "Contribution": return try chartDecoded(Contribution.self, c)
        case "DateRange": return try chartDecoded(DateRange.self, c)
        case "YearMeta": return try chartDecoded(YearMeta.self, c)
        case "UsagePayload": return try chartDecoded(UsagePayload.self, c)
        case "TraceBucket": return try chartDecoded(TraceBucket.self, c)
        case "TokensPerMin": return try chartDecoded(TokensPerMin.self, c)
        default: break
        }
    default: break
    }
    throw NSError(domain: "Charts", code: 1, userInfo: [NSLocalizedDescriptionKey: "unknown charts fixture operation or type"])
}

func charts(_ fixtures: URL) throws -> [String: Any] {
    let data = try Data(contentsOf: fixtures.appendingPathComponent("charts.json"))
    guard let root = try JSONSerialization.jsonObject(with: data) as? [String: Any],
          let cases = root["cases"] as? [[String: Any]],
          cases.allSatisfy({ $0["id"] is String && $0["op"] is String }) else {
        throw NSError(domain: "Charts", code: 1, userInfo: [NSLocalizedDescriptionKey: "invalid charts fixture cases"])
    }
    var output: [[String: Any]] = []
    for c in cases {
        var result: [String: Any]
        do { result = try chartCase(c) }
        catch let error as NSError where error.domain == "Charts" { throw error }
        catch { result = ["error": chartError(error)] }
        result["id"] = c["id"]!
        output.append(result)
    }
    return ["cases": output]
}
