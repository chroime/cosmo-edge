#!/bin/bash

cosmo_set_governor() {
    local path="$1" governor="$2" actual
    if [ ! -e "$path" ]; then
        printf 'WARNING: governor path is missing: %s\n' "$path" >&2
        return 1
    fi
    if [ ! -w "$path" ]; then
        printf 'WARNING: governor path is not writable: %s\n' "$path" >&2
        return 1
    fi
    if ! printf '%s\n' "$governor" >"$path"; then
        printf 'WARNING: failed to set governor %s to %s\n' "$path" "$governor" >&2
        return 1
    fi
    if ! actual="$(cat "$path")"; then
        printf 'WARNING: cannot read back governor: %s\n' "$path" >&2
        return 1
    fi
    if [ "$actual" != "$governor" ]; then
        printf 'WARNING: governor readback mismatch: %s requested=%s actual=%s\n' \
            "$path" "$governor" "$actual" >&2
        return 1
    fi
    printf '%s=%s\n' "$path" "$actual"
}

cosmo_apply_rk3576_performance() {
    local policy status=0 cpu_policies=0
    if ! cosmo_is_rk3576; then
        printf 'Performance profile skipped: live device-tree does not identify RK3576\n'
        return 0
    fi
    if [ "${COSMO_RK3576_PERFORMANCE:-1}" = 0 ]; then
        printf 'Performance profile skipped (COSMO_RK3576_PERFORMANCE=0)\n'
        return 0
    fi

    cosmo_set_governor /sys/class/devfreq/27700000.npu/governor performance || status=1
    cosmo_set_governor /sys/class/devfreq/dmc/governor performance || status=1
    for policy in /sys/devices/system/cpu/cpufreq/policy*; do
        [ -d "$policy" ] || continue
        cpu_policies=$((cpu_policies + 1))
        cosmo_set_governor "$policy/scaling_governor" performance || status=1
    done
    if [ "$cpu_policies" -eq 0 ]; then
        printf 'WARNING: no CPU frequency policies found\n' >&2
        status=1
    fi
    return "$status"
}

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    set -eu
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    # shellcheck source=common.sh
    . "${SCRIPT_DIR}/common.sh"
    cosmo_apply_rk3576_performance
fi
