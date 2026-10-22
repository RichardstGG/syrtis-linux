"""Structure-only Swift preflight plus separate C++ regression assertions."""

import copy
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def verify_structure(output, fixture):
    source = json.loads(Path(fixture).read_text())['cases']
    result = json.loads(Path(output).read_text())
    assert set(result) == {'cases'} and isinstance(result['cases'], list)
    assert [c['id'] for c in result['cases']] == [c['id'] for c in source]
    for c in result['cases']:
        assert set(c) in ({'id', 'value'}, {'id', 'error'}), c['id']
        if 'value' in c:
            assert isinstance(c['value'], dict), c['id']
        else:
            error = c['error']
            assert error['kind'] in {'keyNotFound', 'valueNotFound', 'typeMismatch', 'dataCorrupted'}
            assert isinstance(error['path'], list) and all(isinstance(p, str) for p in error['path'])
            if 'field' in error:
                assert isinstance(error['field'], str)


def verify_cpp(output):
    cases = {c['id']: c for c in json.loads(Path(output).read_text())['cases']}
    values = {id: c['value'] for id, c in cases.items() if 'value' in c}
    for id in ('foreign-null', 'foreign-number', 'foreign-object', 'malformed', 'object',
               'duplicate', 'duplicate-canonical', 'unknown-target', 'empty-source',
               'unassigned', 'missing-model', 'extra-key', 'wrong-model'):
        c = values['codec-' + id]
        assert c['isWritable'] is False and c['records'] == []
        assert c['updatedRaw'] is None and c['suggestedRaw'] is None and c['replacedRaw'] is None
    valid = values['codec-valid']
    assert [s['kind'] for s in valid['states']] == ['assigned', 'assigned', 'excluded', 'unassigned']
    assert valid['declares'] is True
    assert valid['updatedRaw'] == valid['suggestedRaw']
    assert '\\/' in valid['updatedRaw']  # Foundation's canonical string codec
    assert values['codec-source-outside-registry']['isWritable'] is True
    assert values['codec-invalid-update']['updatedRaw'] is None
    assert values['codec-invalid-target']['updatedRaw'] is None
    assert values['codec-replacement-duplicate']['replacedRaw'] is None
    assert values['codec-last-update-wins']['states'][0]['kind'] == 'assigned'
    updated = json.loads(values['codec-last-update-wins']['updatedRaw'])
    assert next(r for r in updated if r['client'] == 'synthetic' and r['model'] is None)['state'] == 'excluded'
    expected = {'empty': 'unavailable', 'single': 'unavailable', 'flat': 'notMoved', 'fall': 'notMoved',
                'unaccounted': 'unaccounted', 'insufficient': 'insufficient', 'threshold': 'ratio',
                'tokens': 'tokensOnly', 'cost': 'costOnly', 'reset-runs': 'insufficient'}
    for id, kind in expected.items():
        assert values['row-' + id]['row']['kind'] == kind, id
    assert values['row-boundaries']['row']['costPerTenth'] == 2
    assert values['row-saturation']['row']['tokensPerTenth'] == 2**63 - 1
    assert values['row-negative-overflow']['row']['tokensPerTenth'] == 2**63 - 1
    assert values['row-exact-int']['row']['tokensPerTenth'] == 9007199254740992  # Swift Double ratio rounding
    expected = {'empty': 'unavailable', 'flat': 'notMoved', 'unaccounted': 'unaccounted',
                'insufficient': 'insufficient', 'low-coverage': 'insufficient', 'few': 'tooFewCycles',
                'ratio': 'ratio', 'tokens': 'tokensOnly', 'cost': 'costOnly', 'few-each': 'tooFewCycles',
                'token-spread': 'spread', 'cost-spread': 'spread', 'multi-run-reject': 'insufficient',
                'multi-run-admit': 'ratio', 'coverage-threshold': 'ratio', 'undeclared': 'undeclared'}
    for id, kind in expected.items():
        assert values['aggregate-' + id]['row']['kind'] == kind, id
    assert values['aggregate-token-separate-pool']['row']['tokensPerTenth'] == 100
    assert values['aggregate-price-separate-pool']['row']['costPerTenth'] == 2
    h = values['history-mixed']
    assert len(h['cycles']) == 1 and h['cycles'][0]['resetAtMs'] == 18000000
    assert h['cycles'][0]['sampleCount'] == 4 and h['cycles'][0]['usedPercent'] == 21
    assert h['rows'][0]['spanTokens'] == h['spans'][0]['tokens']
    # The observed span includes the drifted last sample beyond reset; the
    # whole-window row excludes reset itself. These are intentionally different.
    assert h['rows'][0]['spanTokens'] == 4 * 49
    assert all(h['rows'][0][key] for key in ('otherHasAssigned', 'otherHasExcluded', 'otherHasUnattributed'))
    assert h['rows'][0]['mineTokens'] == 3 * 49
    assert h['rows'][0]['mineTokensExCacheRead'] == 3 * 19
    assert h['rows'][0]['mineBreakdown']['total'] == h['rows'][0]['mineTokens']
    assert values['history-shortened-duration']['cycles'][0]['evidenceStartMs'] == 0
    assert values['history-mode-tie']['cycles'][0]['resetAtMs'] == 18000000
    cap = values['history-cap']
    assert len(cap['cycles']) == 35 and len(cap['considered']) == len(cap['rows']) == 32
    for v in values.values():
        if 'rows' in v:
            assert v['spans'] == [dict(tokens=r['spanTokens'], cost=r['spanCost']) for r in v['rows']]
        if 'curve' in v:
            points, curve = v['samplePoints'], v['curve']
            assert len(curve) == (1 + (len(points) - 1) * 8 if len(points) >= 2 else 0)
            assert len(v['bars']) == len(v['hits'])
            for index in range(len(points) - 1):
                lo, hi = sorted([points[index]['y'], points[index + 1]['y']])
                for point in curve[index * 8:(index + 1) * 8 + 1]:
                    assert math.isfinite(point['y']) and lo - 1e-9 <= point['y'] <= hi + 1e-9
    assert all(b['isEmpty'] for b in values['geometry-cache-only']['bars'])
    assert values['geometry-before-start']['bars'] == []
    # These classifications and paths come directly from JsonDecoder.h.
    for id, kind, path in [('bool-null', 'typeMismatch', ['Index 0', 'isTurnStart']),
                           ('int-fraction', 'dataCorrupted', []),
                           ('int-null', 'valueNotFound', ['Index 0', 'input']),
                           ('string-type', 'typeMismatch', ['Index 0', 'modelId'])]:
        assert cases['decode-' + id]['error'] == dict(kind=kind, path=path)


def main():
    harness, work = sys.argv[1:]
    Path(work).mkdir(parents=True, exist_ok=True)
    fixture = Path(__file__).resolve().parents[1] / 'fixtures/attribution.json'
    env = dict(os.environ, TZ='Asia/Taipei')
    with tempfile.TemporaryDirectory(dir=work) as scratch:
        scratch = Path(scratch)
        out = scratch / 'out'
        subprocess.run([harness, str(fixture.parent), str(out), 'attribution'], env=env, check=True)
        output = out / 'attribution.actual.json'
        verify_structure(output, fixture)
        verify_cpp(output)
        local = scratch / 'fixtures'
        local.mkdir()
        baseline = dict(id='ceiling', op='codec', input=dict(records=[], queries=[], subscription='claude'))
        # Construct size ceilings locally; never create an oracle golden.
        for count, writable in [(128, True), (129, False)]:
            c = copy.deepcopy(baseline)
            c['input']['raw'] = json.dumps([dict(client=f'synthetic-{i}', provider='p', model=None, state='excluded') for i in range(count)])
            (local / 'attribution.json').write_text(json.dumps(dict(cases=[c])))
            subprocess.run([harness, str(local), str(out), 'attribution'], env=env, check=True)
            assert json.loads(output.read_text())['cases'][0]['value']['isWritable'] is writable
        for size, writable in [(65536, True), (65537, False)]:
            c = copy.deepcopy(baseline)
            c['input']['raw'] = '[]' + ' ' * (size - 2)
            (local / 'attribution.json').write_text(json.dumps(dict(cases=[c])))
            subprocess.run([harness, str(local), str(out), 'attribution'], env=env, check=True)
            assert json.loads(output.read_text())['cases'][0]['value']['isWritable'] is writable
        for bad in ({}, {'cases': {}}, {'cases': [baseline, baseline]},
                    {'cases': [dict(baseline, op='unknown')]}):
            (local / 'attribution.json').write_text(json.dumps(bad))
            assert subprocess.run([harness, str(local), str(out), 'attribution'], env=env, capture_output=True).returncode != 0
    print('attribution C++ self-test OK (codec, estimates, history, geometry, limits, fixture guards)')


if __name__ == '__main__':
    main()
