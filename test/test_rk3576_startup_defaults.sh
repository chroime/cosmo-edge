#!/bin/bash
set -euo pipefail

repo="$(cd "$(dirname "$0")/.." && pwd -P)"
root="$(mktemp -d)"
trap 'rm -rf -- "$root"' EXIT
export COSMO_INSTALL_DIR="$root/install"
unset COSMO_RKNN_CORE_MODE COSMO_RK3576_PERFORMANCE
. "$repo/scripts/common.sh"
. "$repo/scripts/rk3576-performance.sh"

printf 'vendor,board\0rockchip,rk3576\0' >"$root/compatible"
cosmo_is_rk3576 "$root/compatible"
for token in rockchip,rk3588 rockchip,rv1126b vendor,rk3576-board rockchip,rk35760; do
    printf '%s\0' "$token" >"$root/other"
    if cosmo_is_rk3576 "$root/other"; then
        echo "Wrong chip accepted: $token" >&2
        exit 1
    fi
done
printf 'rockchip,rk3576' >"$root/truncated"
! cosmo_is_rk3576 "$root/truncated"
! cosmo_is_rk3576 "$root/missing"

mkdir -p "$COSMO_INSTALL_DIR/scripts"
export COSMO_TEST_CALLS="$root/calls"
cat >"$COSMO_INSTALL_DIR/scripts/rk3576-performance.sh" <<'EOF'
#!/bin/bash
printf 'called\n' >>"$COSMO_TEST_CALLS"
exit "${COSMO_TEST_STATUS:-0}"
EOF
chmod +x "$COSMO_INSTALL_DIR/scripts/rk3576-performance.sh"

# These overrides isolate hardware access without adding test-only production
# environment variables. No test executes a command against real sysfs.
cosmo_is_rk3576() { return 1; }
configure_rk3576_runtime
test "${COSMO_RKNN_CORE_MODE+x}" != x
test ! -e "$COSMO_TEST_CALLS"
cosmo_is_rk3576() { return 0; }
configure_rk3576_runtime >"$root/default.log"
test "$COSMO_RKNN_CORE_MODE" = dual
test "$(bash -c 'printf "%s" "$COSMO_RKNN_CORE_MODE"')" = dual
grep -Fq 'package-default' "$root/default.log"
test "$(wc -l <"$COSMO_TEST_CALLS" | tr -d ' ')" = 1
for mode in auto core0 split ''; do
    COSMO_RKNN_CORE_MODE="$mode"
    configure_rk3576_runtime >/dev/null
    test "$COSMO_RKNN_CORE_MODE" = "$mode"
done

rm "$COSMO_TEST_CALLS"
unset COSMO_RKNN_CORE_MODE
COSMO_RK3576_PERFORMANCE=0
configure_rk3576_runtime >/dev/null
test "$COSMO_RKNN_CORE_MODE" = dual
test ! -e "$COSMO_TEST_CALLS"
unset COSMO_RK3576_PERFORMANCE
export COSMO_TEST_STATUS=1
configure_rk3576_runtime >"$root/failure.log"
grep -Fq 'WARNING: performance profile was not fully applied' "$root/failure.log"
rm "$COSMO_INSTALL_DIR/scripts/rk3576-performance.sh"
configure_rk3576_runtime >"$root/missing.log"
grep -Fq 'WARNING: performance script is unavailable' "$root/missing.log"

# Verify real write/readback handling against a private regular file.
printf 'schedutil\n' >"$root/governor"
cosmo_set_governor "$root/governor" performance >/dev/null
grep -Fxq performance "$root/governor"
! cosmo_set_governor "$root/absent" performance 2>"$root/write-error"
grep -Fq 'governor path is missing' "$root/write-error"
(
    cat() { printf 'schedutil\n'; }
    ! cosmo_set_governor "$root/governor" performance 2>"$root/readback-error"
    grep -Fq 'governor readback mismatch' "$root/readback-error"
)
(
    cosmo_set_governor() { echo 'Unexpected hardware write' >&2; exit 1; }
    cosmo_is_rk3576() { return 1; }
    cosmo_apply_rk3576_performance >/dev/null
    cosmo_is_rk3576() { return 0; }
    COSMO_RK3576_PERFORMANCE=0
    cosmo_apply_rk3576_performance >/dev/null
)

echo 'RK3576 startup default tests passed'
