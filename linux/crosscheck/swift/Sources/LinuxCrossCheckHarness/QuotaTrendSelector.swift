import Foundation
import TokenBarCore

private struct QuotaTrendFixture: Decodable {
    struct HeatmapRow: Decodable {
        let id: String
        let timeZone: String
        let points: [QuotaCurvePoint]
    }

    struct TrendRow: Decodable {
        struct Sample: Decodable {
            let atMs: Int64
            let usedPercent: Double
        }

        let id: String
        let usedPercent: Double
        let windowStartMs: Int64
        let windowEndMs: Int64
        let nowMs: Int64
        let samples: [Sample]
    }

    let heatmap: [HeatmapRow]
    let trend: [TrendRow]
}

private enum QuotaTrendFixtureError: Error {
    case invalidTimeZone, invalidID
}

func quotaTrendSelector(_ fixturesDir: URL) throws -> [String: Any] {
    let data = try Data(contentsOf: fixturesDir.appendingPathComponent("quota-trend.json"))
    let fixture = try JSONDecoder().decode(QuotaTrendFixture.self, from: data)
    var output: [String: Any] = [:]
    func insert(_ id: String, _ value: Any) throws {
        guard !id.isEmpty, output[id] == nil else { throw QuotaTrendFixtureError.invalidID }
        output[id] = value
    }

    for row in fixture.heatmap {
        guard let zone = TimeZone(identifier: row.timeZone) else {
            throw QuotaTrendFixtureError.invalidTimeZone
        }
        let heatmap = QuotaHeatmapFold.build(
            points: row.points, calendar: Calendar(identifier: .gregorian), timeZone: zone)
        try insert(row.id, [
            "cells": heatmap.cells,
            "peak": heatmap.peak,
            "total": heatmap.total,
            "unplacedPercent": heatmap.unplacedPercent,
            "observedDays": heatmap.observedDays,
            "isEmpty": heatmap.isEmpty,
            "hasMovement": heatmap.hasMovement,
        ] as [String: Any])
    }

    for row in fixture.trend {
        let samples = row.samples.map { QuotaSample(atMs: $0.atMs, usedPercent: $0.usedPercent) }
        guard let trend = QuotaTrendFold.trend(
            usedPercent: row.usedPercent, windowStartMs: row.windowStartMs,
            windowEndMs: row.windowEndMs, nowMs: row.nowMs, samples: samples)
        else {
            try insert(row.id, NSNull())
            continue
        }
        let direction: String
        switch trend.direction {
        case .rising: direction = "rising"
        case .falling: direction = "falling"
        case .flat: direction = "flat"
        }
        try insert(row.id, [
            "direction": direction,
            "projectedUsedPercent": trend.projectedUsedPercent,
            "projectedDeltaPercent": trend.projectedDeltaPercent,
            "runsOutEarly": trend.runsOutEarly,
        ] as [String: Any])
    }
    return output
}
