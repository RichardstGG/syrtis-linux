import Foundation
import TokenBarCore

// Reflection only serializes shipping values. All decoding, validation and card
// qualification below is performed by TokenBarCore itself.
private func agentJSON(_ value: Any) -> Any {
    let mirror = Mirror(reflecting: value)
    if mirror.displayStyle == .optional {
        return mirror.children.first.map { agentJSON($0.value) } ?? NSNull()
    }
    if let v = value as? UsagePaceState { return v.rawValue }
    if let v = value as? UsagePaceDurationSource { return v.rawValue }
    if let v = value as? UsagePaceUnavailableReason { return v.rawValue }
    if let v = value as? QuotaCurveDurationSource { return v.rawValue }
    if let v = value as? QuotaCurveSampleOrigin { return v.rawValue }
    if value is String || value is Bool || value is Int || value is Int64 || value is UInt64 || value is Double { return value }
    switch mirror.displayStyle {
    case .collection: return mirror.children.map { agentJSON($0.value) }
    case .struct: return Dictionary(uniqueKeysWithValues: mirror.children.map { ($0.label!, agentJSON($0.value)) })
    default: return value
    }
}

private func agentCard(_ value: AgentUsageSnapshot) -> [String: Any] {
    let instructions: [String: Any]
    switch value.setupInstructions {
    case .claudeSetupToken: instructions = ["kind": "claudeSetupToken"]
    case .providerMessage(let message): instructions = ["kind": "providerMessage", "message": message]
    case .none: instructions = ["kind": "none"]
    }
    return ["isSetupPlaceholder": value.isSetupPlaceholder,
            "setupBadgeKey": agentJSON(value.setupBadgeKey as Any),
            "setupInstructions": instructions,
            "historyReadAccountKey": agentJSON(value.historyReadAccountKey as Any),
            "rawCardWindows": agentJSON(value.rawCardWindows)]
}

private func decodeAgent<T: Decodable>(_ type: T.Type, _ data: Data, _ mode: String) throws -> [String: Any] {
    if mode == "optional" {
        let result: T? = try TBCore.decodeOptionalEnvelope(data)
        return ["value": agentJSON(result as Any)]
    }
    let value: T = mode == "envelope" ? try TBCore.decodeEnvelope(data) : try JSONDecoder().decode(type, from: data)
    var out: [String: Any] = ["value": agentJSON(value)]
    if let v = value as? AgentUsagePayload { out["configuredClientIds"] = v.configuredClientIds }
    if let v = value as? AgentUsageSnapshot { out["card"] = agentCard(v) }
    if let v = value as? PaceStatus { out["historyKey"] = agentJSON(v.historyKey as Any) }
    return out
}

private func agentError(_ error: Error) -> [String: Any] {
    if case TBCoreError.bridge(let message) = error { return ["kind": "bridge", "message": message] }
    func context(_ kind: String, _ c: DecodingError.Context, field: String? = nil) -> [String: Any] {
        var out: [String: Any] = ["kind": kind, "path": c.codingPath.map(\.stringValue)]
        if let field { out["field"] = field }
        if c.debugDescription.hasPrefix("successful FFI envelope") { out["description"] = c.debugDescription }
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

func agentUsage(_ fixtures: URL) throws -> [String: Any] {
    func invalid(_ message: String) -> NSError {
        NSError(domain: "AgentUsageHarness", code: 1, userInfo: [NSLocalizedDescriptionKey: message])
    }
    func bytes(_ value: Any) throws -> Data {
        try JSONSerialization.data(withJSONObject: value, options: [.fragmentsAllowed, .sortedKeys])
    }
    let data = try Data(contentsOf: fixtures.appendingPathComponent("agent-usage.json"))
    guard let root = try JSONSerialization.jsonObject(with: data) as? [String: Any],
          let cases = root["cases"] as? [[String: Any]] else { throw invalid("invalid agent-usage fixture cases") }
    var output: [[String: Any]] = []
    var ids = Set<String>()
    for c in cases {
        let mode = c["mode"] as? String ?? "bare"
        let op = c["op"] as? String ?? "decode"
        guard let id = c["id"] as? String, ids.insert(id).inserted, let type = c["type"] as? String,
              c["input"] != nil || c["rawJSON"] is String,
              ["bare", "envelope", "optional"].contains(mode) else { throw invalid("invalid agent-usage fixture case") }
        let input = try (c["rawJSON"] as? String).map { Data($0.utf8) } ?? bytes(c["input"]!)
        var result: [String: Any]
        do {
            switch op {
            case "qualify":
                guard var snapshot = c["input"] as? [String: Any], var windows = snapshot["windows"] as? [[String: Any]] else { throw invalid("invalid qualification input") }
                if let rawOffsets = c["resetOffsetsSeconds"] {
                    guard let offsets = rawOffsets as? [Any], offsets.count == windows.count else { throw invalid("invalid reset offsets") }
                    let now = Date()
                    let formatter = ISO8601DateFormatter()
                    formatter.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
                    for index in offsets.indices {
                        if offsets[index] is NSNull { continue }
                        guard let offset = offsets[index] as? Double else { throw invalid("invalid reset offset") }
                        windows[index]["resetsAt"] = formatter.string(from: now.addingTimeInterval(offset))
                    }
                    snapshot["windows"] = windows
                }
                let value = try JSONDecoder().decode(AgentUsageSnapshot.self, from: bytes(snapshot))
                func names(_ windows: [UsageWindow]) -> [[String: String]] { windows.map { ["cardId": $0.cardId, "label": $0.label] } }
                // Only names leave this operation: wall-clock timestamps stay
                // synthetic inputs, and the shipping public card view owns all
                // qualification tiers. Offset fixtures leave a rounding margin.
                result = ["value": ["raw": names(value.rawCardWindows), "qualified": names(value.uniqueCardWindows)]]
            case "adoptHistory":
                let primary = try JSONDecoder().decode(AgentUsageSnapshot.self, from: input)
                let value = primary.adoptingHistory(of: try JSONDecoder().decode(AgentUsageSnapshot.self, from: bytes(c["other"]!)))
                result = ["value": agentJSON(value), "card": agentCard(value)]
            case "replacePace":
                let primary = try JSONDecoder().decode(UsageWindow.self, from: input)
                let value = primary.replacingPace(from: try JSONDecoder().decode(UsageWindow.self, from: bytes(c["other"]!)))
                result = ["value": agentJSON(value)]
            case "replaceIdentity":
                let primary = try JSONDecoder().decode(AgentUsageSnapshot.self, from: input)
                let value = primary.replacingIdentity(try JSONDecoder().decode(AgentIdentity?.self, from: bytes(c["other"]!)))
                result = ["value": agentJSON(value), "card": agentCard(value)]
            case "replaceAgents":
                let primary = try JSONDecoder().decode(AgentUsagePayload.self, from: input)
                let value = primary.replacingAgents(try JSONDecoder().decode([AgentUsageSnapshot].self, from: bytes(c["other"]!)))
                result = ["value": agentJSON(value), "configuredClientIds": value.configuredClientIds]
            case "decode":
                switch type {
                case "HistoricalPace": result = try decodeAgent(HistoricalPace.self, input, mode)
                case "PaceStatus": result = try decodeAgent(PaceStatus.self, input, mode)
                case "UsageWindow": result = try decodeAgent(UsageWindow.self, input, mode)
                case "AgentIdentity": result = try decodeAgent(AgentIdentity.self, input, mode)
                case "CreditsSnapshot": result = try decodeAgent(CreditsSnapshot.self, input, mode)
                case "AgentUsageTransportDiagnostic": result = try decodeAgent(AgentUsageTransportDiagnostic.self, input, mode)
                case "AgentUsageSnapshot": result = try decodeAgent(AgentUsageSnapshot.self, input, mode)
                case "AgentUsagePayload": result = try decodeAgent(AgentUsagePayload.self, input, mode)
                case "QuotaProviderIds": result = try decodeAgent(QuotaProviderIds.self, input, mode)
                case "QuotaCurvePoint": result = try decodeAgent(QuotaCurvePoint.self, input, mode)
                case "QuotaCurveCoverage": result = try decodeAgent(QuotaCurveCoverage.self, input, mode)
                case "QuotaCurve": result = try decodeAgent(QuotaCurve.self, input, mode)
                default: throw invalid("unknown agent-usage fixture type")
                }
            default: throw invalid("unknown agent-usage operation")
            }
        } catch let error as NSError where error.domain == "AgentUsageHarness" { throw error }
        catch { result = ["error": agentError(error)] }
        result["id"] = id
        output.append(result)
    }
    return ["cases": output]
}
