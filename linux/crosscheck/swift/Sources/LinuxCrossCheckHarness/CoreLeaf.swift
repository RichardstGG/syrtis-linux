import Foundation
import TokenBarCore

private func strings(_ value: Any?) -> [String] { value as? [String] ?? [] }
private func text(_ value: Any?) -> String { value as? String ?? "" }
private func nullable<T>(_ value: T?) -> Any { value.map { $0 as Any } ?? NSNull() }

// Only the harness process's four synthetic preferences are changed, and every
// original value is restored. Shipping calculations still use UserDefaults.standard.
private func withPreferences<T>(_ values: [String: Any], _ body: () -> T) -> T {
    let defaults = UserDefaults.standard
    let keys = [ClientRegistry.tabOrderKey, ClientRegistry.tabHiddenKey,
                ClientRegistry.limitsHiddenKey, "tokenbar.limits.order"]
    let saved = keys.map { ($0, defaults.object(forKey: $0)) }
    let fields = ["order", "hidden", "limitsHidden", "legacyOrder"]
    for (key, field) in zip(keys, fields) {
        defaults.removeObject(forKey: key)
        if let value = values[field] as? String { defaults.set(value, forKey: key) }
    }
    defer {
        for (key, value) in saved {
            defaults.removeObject(forKey: key)
            if let value { defaults.set(value, forKey: key) }
        }
    }
    return body()
}

private func registryCase(_ c: [String: Any]) -> [String: Any] {
    let present = strings(c["present"]), quotas = strings(c["quotaIds"])
    let hidden = Set(strings(c["tabHidden"])), limits = Set(strings(c["limitsHidden"]))
    let raw = text(c["raw"]), order = text(c["orderRaw"])
    var out: [String: Any] = [
        "parseIdList": ClientRegistry.parseIdList(raw),
        "parseIdSet": ClientRegistry.parseIdSet(raw).sorted(),
        "tabClients": ClientRegistry.tabClients(present: present, quotaIds: quotas),
        "quotaClients": ClientRegistry.quotaClients(present: present, quotaIds: quotas, tabHidden: hidden, orderRaw: order),
        "withGroupMembers": ClientRegistry.withGroupMembers(hidden).sorted(),
        "hiddenTabClients": ClientRegistry.hiddenTabClients(hidden).sorted(),
        "quotaExcludedClients": ClientRegistry.quotaExcludedClients(tabHidden: hidden, limitsHidden: limits).sorted(),
        "knownLimitsClients": ClientRegistry.knownLimitsClients(present: present, quotaIds: quotas, placeholders: Set(strings(c["placeholders"]))),
        "orderedClients": ClientRegistry.orderedClients(present, orderRaw: order),
        "displayClients": ClientRegistry.displayClients(present: present, hiddenRaw: text(c["hiddenRaw"]), orderRaw: order),
        "reorder": ClientRegistry.reorder(strings(c["list"]), from: text(c["fromId"]), to: text(c["toId"])),
        "mergeReorder": ClientRegistry.mergeReorder(full: strings(c["full"]), visible: strings(c["visible"]), from: text(c["fromId"]), to: text(c["toId"])),
    ]
    withPreferences(c["preferences"] as? [String: Any] ?? [:]) {
        out["settings"] = [
            "hiddenClients": ClientRegistry.hiddenClients().sorted(),
            "hiddenLimitsClients": ClientRegistry.hiddenLimitsClients().sorted(),
            "hiddenTabClients": ClientRegistry.hiddenTabClients().sorted(),
            "quotaExcludedClients": ClientRegistry.quotaExcludedClients().sorted(),
            "orderedClients": ClientRegistry.orderedClients(present),
            "displayClients": ClientRegistry.displayClients(present: present),
        ] as [String: Any]
        ClientRegistry.migrateLegacyOrderKey()
        out["migratedOrder"] = nullable(UserDefaults.standard.string(forKey: ClientRegistry.tabOrderKey))
        ClientRegistry.migrateLegacyOrderKey()
        out["migratedOrderAgain"] = nullable(UserDefaults.standard.string(forKey: ClientRegistry.tabOrderKey))
    }
    return out
}

private struct WindowCase: Decodable {
    let reset: Int64?
    let duration: Int64?
    let now: Int64
    let first: Int64?
    let probeReset: Int64
    let messages: [WindowMessage]
}
private struct DayEntry: Decodable {
    let date: String
    let tokens: Int64
    let cost: Double
    let intensity: Int
    let hasMessages: Bool
    var perDay: PerDay {
        PerDay(date: date, tokens: tokens, cost: cost, intensity: intensity, hasMessages: hasMessages)
    }
}
private struct GridCase: Decodable {
    let year: String
    let entries: [DayEntry]
}
private func dayJSON(_ day: ISODay) -> [String: Any] {
    ["number": day.number, "iso": day.iso, "weekday": day.weekday]
}

func coreLeaf(_ fixtures: URL) throws -> [String: Any] {
    let data = try Data(contentsOf: fixtures.appendingPathComponent("core-leaf.json"))
    guard let root = try JSONSerialization.jsonObject(with: data) as? [String: Any],
          ["ids", "labels", "registry", "grouping", "scopes", "windows", "grids", "days", "dayNumbers"]
            .allSatisfy({ root[$0] is [Any] }) else {
        throw NSError(domain: "CoreLeaf", code: 1, userInfo: [NSLocalizedDescriptionKey: "invalid core-leaf fixture arrays"])
    }
    var out: [String: Any] = [
        "allIds": ClientRegistry.allIds,
        "keys": ["order": ClientRegistry.tabOrderKey, "hidden": ClientRegistry.tabHiddenKey, "limitsHidden": ClientRegistry.limitsHiddenKey],
        "subscriptionLabelAliases": ClientRegistry.subscriptionLabelAliases,
    ]
    out["styles"] = (ClientRegistry.allIds + strings(root["ids"])).map { id -> [String: Any] in
        let style = ClientRegistry.style(id)
        return ["id": style.id, "displayName": style.displayName, "color": style.color,
                "shortName": ClientRegistry.shortName(id), "canonicalClient": ClientRegistry.canonicalClient(id),
                "quotaOwner": ClientRegistry.quotaOwner(id), "tabSlice": ClientRegistry.tabSlice(id),
                "tabLabel": ClientRegistry.tabLabel(id), "tabDisplayName": ClientRegistry.tabDisplayName(id)]
    }
    out["labels"] = strings(root["labels"]).map { ClientRegistry.clientId(forSubscriptionLabel: $0) }
    out["registry"] = (root["registry"] as! [[String: Any]]).map(registryCase)
    out["grouping"] = strings(root["grouping"]).map(ModelGrouping.groupID)
    out["scopes"] = (root["scopes"] as! [[String: Any]]).map {
        ModelScope.covers(text($0["scope"]), modelId: text($0["model"]))
    }
    let decoder = JSONDecoder()
    let windowData = try JSONSerialization.data(withJSONObject: root["windows"]!)
    out["windows"] = try decoder.decode([WindowCase].self, from: windowData).map { c -> [String: Any] in
        let resolution = WindowResolver.resolve(resetsAtMs: c.reset, durationMs: c.duration, now: c.now, firstUsageAfterReset: c.first)
        var result: [String: Any] = ["firstUsageAfterReset": nullable(WindowResolver.firstUsageAfterReset(messages: c.messages, resetMs: c.probeReset))]
        switch resolution {
        case .active(let start, let end): result.merge(["kind": "active", "start": start, "end": end]) { _, new in new }
        case .inferred(let start, let end): result.merge(["kind": "inferred", "start": start, "end": end]) { _, new in new }
        case .idle: result["kind"] = "idle"
        case .unavailable: result["kind"] = "unavailable"
        }
        return result
    }
    let gridData = try JSONSerialization.data(withJSONObject: root["grids"]!)
    out["grids"] = try decoder.decode([GridCase].self, from: gridData).map { c -> [String: Any] in
        var perDay: [String: PerDay] = [:]
        for entry in c.entries { perDay[entry.date] = entry.perDay }
        let grid = buildGrid(year: c.year, perDayMap: perDay)
        let cells = grid.cells.map { cell -> [String: Any] in
            ["col": cell.col, "row": cell.row, "date": cell.date, "inYear": cell.inYear,
             "active": cell.active, "tokens": cell.tokens, "cost": cell.cost]
        }
        return ["cols": grid.cols, "rows": grid.rows, "maxTokens": grid.maxTokens,
                "cells": cells, "perDayIsActive": c.entries.map { $0.perDay.isActive }]
    }
    out["days"] = strings(root["days"]).map { nullable(ISODay($0).map(dayJSON)) }
    out["dayNumbers"] = (root["dayNumbers"] as! [NSNumber]).map { dayJSON(ISODay(number: $0.intValue)) }
    return out
}
