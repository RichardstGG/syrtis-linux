"""Structure-only oracle preflight; C++ expectations stay in verify_cpp."""

import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from attribution_check import verify_structure


def verify_cpp(output, fixture):
    cases = {c['id']: c for c in json.loads(Path(output).read_text())['cases']}
    values = {id: c['value'] for id, c in cases.items() if 'value' in c}
    source = {c['id']: c['input'] for c in json.loads(Path(fixture).read_text())['cases']}
    assert len(cases) >= 63
    assert len(values['copy']['settings']) == 20
    assert len(values['copy']['breakdown']) == 4
    rows = values['breakdown-mixed']['rows']
    assert [(r['id'], r['tokens'], r['cost']) for r in rows] == [
        ('assigned:claude', 10, 1), ('assigned:cursor', 80, 0), ('excluded', 20, 2), ('unassigned', 7, 0)]
    assert values['breakdown-cancel']['rows'] == []
    assert values['breakdown-saturation']['rows'][0]['tokens'] == 2**63 - 1
    settings = values['settings-mixed']
    assert len(settings['rows']) == 4
    assert settings['rows'][0]['tokens'] == 30  # model overrides do not split Settings sources
    assert settings['rows'][0]['suggestedState'] is None
    assert settings['rows'][2]['id'] == 'synthetic\0'
    assert settings['rows'][2]['providerLabel'] == 'Unspecified provider'
    assert settings['rows'][2]['suggestedState'] == {'kind': 'excluded'}
    assert len(settings['accepted']) == 1
    assert settings['onboarding']['unsuggestedCount'] == 2
    for name in ('loading', 'unavailable', 'empty'):
        assert values['settings-' + name]['pageState'] == name
    assert len(values['settings-onboarding']['proposed']) == 2
    assert len(values['settings-onboarding']['onboarding']['records']) == 2
    assert values['settings-onboarding']['onboarding']['unsuggestedCount'] == 1
    assert values['settings-onboarding']['rows'][2]['state'] == dict(kind='unassigned')
    assert values['settings-onboarding']['accepted'][0]['state'] == dict(kind='assigned', target='claude')
    assert values['settings-router']['proposed'][0]['state'] == dict(kind='assigned', target='codex')
    assert settings['signature'] != values['settings-pending']['signature']
    assert settings['signature'] != values['settings-routing']['signature']
    assert values['settings-cancel-kept']['rows'][0]['tokens'] == 0
    assert values['settings-cancel-kept']['rows'][0]['cost'] == 0
    assert len(values['settings-unicode']['rows']) == 1
    assert values['settings-unicode']['rows'][0]['tokens'] == 12
    assigned = {'own-without-meter': 'cursor', 'quota-owner': 'antigravity', 'router': 'codex',
                'bound-reseller': 'copilot', 'cross-agent': 'codex', 'own-only': 'grok', 'bot-own': 'grok-bot'}
    for name, target in assigned.items():
        assert values['suggestion-' + name]['state'] == dict(kind='assigned', target=target), name
    assert values['suggestion-bound-excluded']['state'] == dict(kind='excluded')
    for name in ('router-ambiguous', 'router-none', 'unknown-source', 'ambiguous',
                 'own-only-no-owner', 'unknown-provider', 'exact-provider'):
        assert values['suggestion-' + name]['state'] is None, name
    targets = values['targets-configured']
    assert targets['subscriptions'] == ['codex', 'grok', 'claude', 'kimi']
    assert targets['routed'] == {'opencode': ['claude', 'grok', 'kimi', 'codex']}
    assert targets['labels'] == ['codex', 'claude', 'antigravity', 'grok', 'micode', 'kimi', None, 'kiro', None, None, 'codex']
    assert values['targets-empty']['subscriptions'] == []
    success = values['accept-success']
    assert success['failure'] is None
    assert len(json.loads(success['confirmedRaw'])) == 4
    assert len(json.loads(success['suggestionsRaw'])) == 1
    for name in ('accept-foreign-confirmed', 'accept-foreign-suggestions', 'accept-noop'):
        v = values[name]
        assert v['confirmedRaw'] == source[name]['confirmedRaw']
        assert v['suggestionsRaw'] == source[name]['suggestionsRaw']
        assert (v['failure'] is None) == (name == 'accept-noop')
    assert values['accept-absent']['failure'] is None
    assert values['accept-absent']['suggestionsRaw'] == '[]'
    assert values['write-valid']['failure'] is None
    assert 'invalid or foreign' in values['write-foreign']['failure']
    assert 'too large or unsupported' in values['write-invalid']['failure']
    assert 'entry limit' in values['write-limit']['failure']
    w = values['window-boundaries']
    assert w['buckets'] == [14, 8, 9]  # -1 / 10 truncates to zero; exact upper bound excluded
    assert w['undatedCount'] == 2 and w['processingTimeMs'] == 3
    assert [(t['target'], t['tokens']) for t in w['assigned']] == [('claude', 119), ('cursor', 8)]
    assert w['excluded']['tokens'] == 5 and w['unassigned']['tokens'] == 9
    assert values['window-zero-width']['buckets'] == [0, 0, 0]
    assert values['window-zero-count']['buckets'] == values['window-negative-count']['buckets'] == []
    assert values['window-overflow']['messages'][0] == dict(tokens=2**63 - 1, tokensExCacheRead=2**63 - 1)
    assert values['window-overflow']['assigned'][0]['tokens'] == 2**63 - 1
    assert values['window-negative-overflow']['messages'][0]['tokens'] == 2**63 - 1
    daily = values['daily-mixed']
    assert daily == values['daily-permuted']
    assert daily['range'] == ['2026-02-27', '2026-02-28', '2026-03-01']
    trend = daily['trend']
    assert trend['targets'] == trend['costOrder'] == ['cursor', 'claude', '__unassigned']
    assert trend['targetsByTokens'] == trend['tokenOrder'] == ['__unassigned', 'claude', 'cursor']
    assert trend['days'][1]['isEmpty'] and trend['days'][1]['byTarget'] == {}
    assert trend['peakTokens'] == 134 and trend['peakCost'] == 11
    assert values['daily-model-group']['points'] == [dict(date='2026-03-01', model='grok-4.1', tokens=12, cost=3, state=dict(kind='assigned', target='grok'))]
    assert [p['state']['kind'] for p in values['daily-raw-override']['points']] == ['assigned', 'excluded']
    assert values['daily-saturation']['points'][0]['tokens'] == 2**63 - 1
    assert len(values['daily-empty']['trend']['days']) == 3
    assert values['daily-cancel']['points'] == []
    cancelled = values['daily-cancel-models']['trend']
    assert cancelled['targets'] == ['__unassigned']
    assert cancelled['days'][-1]['isEmpty'] and cancelled['days'][-1]['byTarget']['__unassigned']['tokens'] == 0
    assert values['daily-zero-days']['trend']['days'] == []
    assert values['daily-invalid-date']['range'] is None
    assert values['daily-leap']['range'] == ['2024-02-28', '2024-02-29', '2024-03-01']
    assert values['daily-dst']['range'] == ['2026-10-31', '2026-11-01', '2026-11-02']
    assert values['daily-negative-only']['trend']['peakCost'] == -2
    assert values['daily-negative-only']['trend']['peakTokens'] == -3
    errors = {
        'window-bool-null': dict(kind='typeMismatch', path=['messages', 'Index 0', 'isTurnStart']),
        'window-int-fraction': dict(kind='dataCorrupted', path=[]),
        'window-int-null': dict(kind='valueNotFound', path=['messages', 'Index 0', 'input']),
        'window-missing-provider': dict(kind='keyNotFound', path=['messages', 'Index 0'], field='providerId'),
        'window-wrong-messages': dict(kind='typeMismatch', path=['messages']),
        'window-missing-count': dict(kind='keyNotFound', path=[], field='undatedCount'),
        'daily-token-null': dict(kind='valueNotFound', path=['Index 0', 'clients', 'Index 0', 'tokens', 'reasoning']),
        'daily-turns': dict(kind='typeMismatch', path=['Index 0', 'turnsByClient', 'claude']),
        'report-cost-missing': dict(kind='keyNotFound', path=['Index 0'], field='cost'),
    }
    for name, error in errors.items():
        assert cases['decode-' + name]['error'] == error, name


def main():
    harness, work = sys.argv[1:]
    Path(work).mkdir(parents=True, exist_ok=True)
    fixture = Path(__file__).resolve().parents[1] / 'fixtures/attribution-ext.json'
    env = dict(os.environ, TZ='Asia/Taipei')
    with tempfile.TemporaryDirectory(dir=work) as scratch:
        scratch = Path(scratch)
        out = scratch / 'out'
        subprocess.run([harness, str(fixture.parent), str(out), 'attribution-ext'], env=env, check=True)
        output = out / 'attribution-ext.actual.json'
        verify_structure(output, fixture)
        verify_cpp(output, fixture)
        # Exercise byte ceilings without storing oversized synthetic fixtures.
        local = scratch / 'fixtures'
        local.mkdir()
        case = dict(id='oversized', op='writeFailure', input=dict(confirmedRaw='[]', records=[
            dict(client='synthetic', provider='p' * 65536, model=None, state='excluded')]))
        (local / 'attribution-ext.json').write_text(json.dumps(dict(cases=[case])))
        subprocess.run([harness, str(local), str(out), 'attribution-ext'], env=env, check=True)
        value = json.loads(output.read_text())['cases'][0]['value']
        assert value['result'] is None and 'too large or unsupported' in value['failure']
        for mutation in ('duplicate', 'unknown-op'):
            bad = copy.deepcopy(case)
            bad['op'] = 'copy' if mutation == 'duplicate' else 'unknown'
            payload = [bad, bad] if mutation == 'duplicate' else [bad]
            (local / 'attribution-ext.json').write_text(json.dumps(dict(cases=payload)))
            result = subprocess.run([harness, str(local), str(out), 'attribution-ext'], env=env, capture_output=True)
            assert result.returncode == 1
    print('attribution-ext C++ regression checks OK; oracle preflight is structure-only')


if __name__ == '__main__':
    main()
