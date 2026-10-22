import Foundation
import TokenBarCore

// Harness-only reflection serializes decoded shipping DTOs, including explicit
// null optionals. No wire DTO or envelope decoder is duplicated here.
private func reportJSON(_ value: Any) -> Any {
    let mirror = Mirror(reflecting: value)
    if mirror.displayStyle == .optional {
        return mirror.children.first.map { reportJSON($0.value) } ?? NSNull()
    }
    if let status = value as? FilterParityStatus { return status.rawValue }
    if value is String || value is Bool || value is Int || value is Int64 || value is UInt64 || value is Double {
        return value
    }
    switch mirror.displayStyle {
    case .collection:
        return mirror.children.map { reportJSON($0.value) }
    case .struct:
        return Dictionary(uniqueKeysWithValues: mirror.children.map { ($0.label!, reportJSON($0.value)) })
    default:
        return value
    }
}

private func decodeReport<T: Decodable>(_ type: T.Type, _ data: Data, _ mode: String) throws -> [String: Any] {
    if mode == "optional" {
        let result: T? = try TBCore.decodeOptionalEnvelope(data)
        return ["value": reportJSON(result as Any)]
    }
    let result: T = mode == "bare" ? try JSONDecoder().decode(type, from: data) : try TBCore.decodeEnvelope(data)
    var out: [String: Any] = ["value": reportJSON(result)]
    if let probe = result as? FilterParityProbe { out["smokeSummary"] = probe.smokeSummary }
    // FilterParityReport.smokeSummary is internal; the probe's public summary
    // exercises its shipping implementation for every status and delta branch.
    return out
}

private func reportError(_ error: Error) -> [String: Any] {
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

func reports(_ fixtures: URL) throws -> [String: Any] {
    let data = try Data(contentsOf: fixtures.appendingPathComponent("reports.json"))
    guard let root = try JSONSerialization.jsonObject(with: data) as? [String: Any],
          let cases = root["cases"] as? [[String: Any]] else {
        throw NSError(domain: "Reports", code: 1, userInfo: [NSLocalizedDescriptionKey: "invalid reports fixture cases"])
    }
    var output: [[String: Any]] = []
    for c in cases {
        let mode = c["mode"] as? String ?? "envelope"
        guard let id = c["id"] as? String, let type = c["type"] as? String,
              c["input"] != nil || c["rawJSON"] is String,
              ["bare", "envelope", "optional"].contains(mode) else {
            throw NSError(domain: "Reports", code: 1, userInfo: [NSLocalizedDescriptionKey: "invalid reports fixture case"])
        }
        let bytes: Data
        if let raw = c["rawJSON"] as? String { bytes = Data(raw.utf8) }
        else { bytes = try JSONSerialization.data(withJSONObject: c["input"]!, options: [.fragmentsAllowed, .sortedKeys]) }
        var result: [String: Any]
        do {
            switch type {
            case "ProbeResult": result = try decodeReport(ProbeResult.self, bytes, mode)
            case "ScanPathNote": result = try decodeReport(ScanPathNote.self, bytes, mode)
            case "ExtraScanPathsResult": result = try decodeReport(ExtraScanPathsResult.self, bytes, mode)
            case "RejectedConfigDir": result = try decodeReport(RejectedConfigDir.self, bytes, mode)
            case "ClaudeConfigDirsResult": result = try decodeReport(ClaudeConfigDirsResult.self, bytes, mode)
            case "RejectedAntigravityAccount": result = try decodeReport(RejectedAntigravityAccount.self, bytes, mode)
            case "AntigravityAccountsResult": result = try decodeReport(AntigravityAccountsResult.self, bytes, mode)
            case "AntigravityCapturedAccount": result = try decodeReport(AntigravityCapturedAccount.self, bytes, mode)
            case "AntigravityBindingResult": result = try decodeReport(AntigravityBindingResult.self, bytes, mode)
            case "AntigravityRemoved": result = try decodeReport(AntigravityRemoved.self, bytes, mode)
            case "AntigravityLoginMarker": result = try decodeReport(AntigravityLoginMarker.self, bytes, mode)
            case "AntigravityAutoCaptureResult": result = try decodeReport(AntigravityAutoCaptureResult.self, bytes, mode)
            case "RejectedKeychainConsent": result = try decodeReport(RejectedKeychainConsent.self, bytes, mode)
            case "KeychainConsentResult": result = try decodeReport(KeychainConsentResult.self, bytes, mode)
            case "CursorSyncConfigResult": result = try decodeReport(CursorSyncConfigResult.self, bytes, mode)
            case "CursorSyncStatus": result = try decodeReport(CursorSyncStatus.self, bytes, mode)
            case "AgentReportEntry": result = try decodeReport(AgentReportEntry.self, bytes, mode)
            case "AgentsReport": result = try decodeReport(AgentsReport.self, bytes, mode)
            case "HourlyReportEntry": result = try decodeReport(HourlyReportEntry.self, bytes, mode)
            case "HourlyReport": result = try decodeReport(HourlyReport.self, bytes, mode)
            case "ModelReportEntry": result = try decodeReport(ModelReportEntry.self, bytes, mode)
            case "ModelReport": result = try decodeReport(ModelReport.self, bytes, mode)
            case "FilterParityAggregate": result = try decodeReport(FilterParityAggregate.self, bytes, mode)
            case "FilterParityReport": result = try decodeReport(FilterParityReport.self, bytes, mode)
            case "FilterParityProbe": result = try decodeReport(FilterParityProbe.self, bytes, mode)
            default: throw NSError(domain: "Reports", code: 1, userInfo: [NSLocalizedDescriptionKey: "unknown reports fixture type"])
            }
        } catch let error as NSError where error.domain == "Reports" { throw error }
        catch { result = ["error": reportError(error)] }
        result["id"] = id
        output.append(result)
    }
    return ["cases": output]
}
