/* C ABI smoke for the Linux port: every tb_* entry point except
 * tb_agent_usage returns a JSON envelope that tb_free releases.
 * tb_agent_usage is excluded because it probes the host's running IDE
 * processes and provider networks; it is covered by the live check in
 * docs/linux-port/evidence. Run through run.sh, which supplies a synthetic HOME. */
#include <stdio.h>
#include <string.h>
#include "ctb.h"

static int fails = 0;

static void check(const char *name, char *p, const char *want) {
    if (!p) {
        printf("FAIL %-34s NULL\n", name);
        fails++;
        return;
    }
    int ok = strstr(p, want) != NULL;
    printf("%s %-34s %.120s\n", ok ? "PASS" : "FAIL", name, p);
    fails += !ok;
    tb_free(p);
}

int main(void) {
    const char *zero_key = "0000000000000000000000000000000000000000000000000000000000000000";

    /* Reports over the synthetic Claude session. */
    check("tb_probe", tb_probe(), "\"ok\":true,\"messages\":1");
    check("tb_client_ids", tb_client_ids(), "\"antigravity-cli\"");
    check("tb_quota_provider_ids", tb_quota_provider_ids(), "\"antigravity\"");
    check("tb_graph(NULL)", tb_graph(NULL), "\"client\":\"claude\"");
    check("tb_graph(\"2026\")", tb_graph("2026"), "\"ok\":true");
    check("tb_refresh_graph(NULL)", tb_refresh_graph(NULL), "\"client\":\"claude\"");
    check("tb_model_report(NULL)", tb_model_report(NULL), "\"ok\":true");
    check("tb_hourly_report(NULL,claude)", tb_hourly_report(NULL, "claude"), "\"input\":100");
    check("tb_agents_report(NULL,NULL)", tb_agents_report(NULL, NULL), "\"ok\":true");
    check("tb_filter_parity_probe", tb_filter_parity_probe(), "\"ok\":true");
    check("tb_usage_trace(600)", tb_usage_trace(600), "\"ok\":true");
    check("tb_tokens_per_min", tb_tokens_per_min(), "\"tokensPerMin\"");
    check("tb_window_usage(NULL,0,1)", tb_window_usage(NULL, 0, 1), "\"ok\":");

    /* Registries: valid input succeeds, malformed input is an error envelope. */
    check("tb_set_extra_scan_paths({})", tb_set_extra_scan_paths("{}"), "\"ok\":true");
    check("tb_set_extra_scan_paths(bad)", tb_set_extra_scan_paths("not json"), "\"ok\":false");
    check("tb_set_claude_config_dirs([])", tb_set_claude_config_dirs("[]"), "\"ok\":true");
    check("tb_set_antigravity_accounts([])", tb_set_antigravity_accounts("[]"), "\"ok\":true");
    check("tb_set_antigravity_binding(NULL)", tb_set_antigravity_binding(NULL), "\"bound\":false");
    check("tb_set_keychain_consent({})", tb_set_keychain_consent("{}"), "\"ok\":true");
    check("tb_set_cursor_sync(off)", tb_set_cursor_sync("{\"enabled\":false}"), "\"ok\":");
    check("tb_cursor_sync(0)", tb_cursor_sync(0), "\"ok\":");

    /* Quota-side entry points without a publication or a macOS Keychain. */
    check("tb_quota_curve(no binding)", tb_quota_curve("codex", NULL, "x", 1), "\"ok\":false");
    check("tb_antigravity_login_marker", tb_antigravity_login_marker(), "marker_unavailable");
    check("tb_antigravity_remove(bad key)", tb_antigravity_remove("nope"), "invalid_key");
    check("tb_antigravity_remove(zero key)", tb_antigravity_remove(zero_key), "\"ok\":");
    check("tb_antigravity_capture", tb_antigravity_capture(), "\"ok\":false");
    check("tb_antigravity_auto_capture([])", tb_antigravity_auto_capture("[]"), "\"ok\":false");

    tb_free(NULL);
    printf("fails=%d\n", fails);
    return fails != 0;
}
