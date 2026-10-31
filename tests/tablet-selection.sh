#!/bin/sh
# Synthetic eligibility only; fixture APK bytes are deliberately not installable.
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
sha=1111111111111111111111111111111111111111
mkdir -p "$tmp/paired/arm64-v8a/native"
{
  printf '%s\n' '# catfood-device-inventory-v1'
  printf 'meta\tdevice_id\ttest-device\tlocal-state\n'
  printf 'meta\tobserved_at\t2026-10-31T00:00:00Z\tclock\n'
  printf 'meta\ttarget\ttablet\tdetected\n'
  printf 'meta\tmodel\tTAB_P10\tgetprop\n'
  printf 'meta\tproduct\tTAB_P10_ROW\tgetprop\n'
  printf 'meta\tabi\tarm64-v8a\tgetprop\n'
  printf 'meta\tandroid_sdk\t35\tgetprop\n'
} > "$tmp/tablet.tsv"
{
  printf 'schema\tcatfood-android-build-plan-v1\n'
  printf 'source_sha\t%s\n' "$sha"
  printf 'requested_target\ttablet\n'
  printf 'package_id\torg.isomorphisms.fastchat.diskstreaming.arena\n'
  printf 'target\tdevice\tabi\tobligation\tacceptance\n'
  printf 'tablet\tTAB_P10\tarm64-v8a\trequired\tnot_run\n'
} > "$tmp/paired/android-plan-tablet.tsv"
printf 'source_commit\t%s\nabi\tarm64-v8a\n' "$sha" > "$tmp/paired/arm64-v8a/native/source-binding.tsv"
printf 'synthetic fixture, not an APK\n' > "$tmp/paired/fastchat-arm64-v8a.apk"
selector="$root/scripts/select-tablet-apk.sh"
if sh "$selector" "$tmp/absent.tsv" "$tmp/paired" "$sha" > "$tmp/out" 2> "$tmp/err"; then
    echo 'missing inventory accepted' >&2; exit 1
fi
grep -Fq 'device inventory unavailable' "$tmp/err"
sh "$selector" "$tmp/tablet.tsv" "$tmp/paired" "$sha" > "$tmp/out"
grep -Fx "$(printf 'status\tELIGIBLE_ONLY')" "$tmp/out"
grep -Fx "$(printf 'signer\tNOT_VERIFIED')" "$tmp/out"
grep -Fx "$(printf 'physical_install\tNOT_RUN')" "$tmp/out"
reject() {
    if sh "$selector" "$tmp/tablet.tsv" "$tmp/paired" "$sha" > "$tmp/out" 2> "$tmp/err"; then
        echo 'invalid tablet fixture accepted' >&2; exit 1
    fi
}
cp "$tmp/tablet.tsv" "$tmp/good.tsv"
sed 's/meta.target.tablet/meta.target.c67/' "$tmp/good.tsv" > "$tmp/tablet.tsv"
reject
sed 's/meta.abi.arm64-v8a/meta.abi.armeabi-v7a/' "$tmp/good.tsv" > "$tmp/tablet.tsv"
reject
sed 's/meta.android_sdk.35/meta.android_sdk.25/' "$tmp/good.tsv" > "$tmp/tablet.tsv"
reject
sed 's/TAB_P10/Unknown/g' "$tmp/good.tsv" > "$tmp/tablet.tsv"
reject
sed 's/detected/planned/' "$tmp/good.tsv" > "$tmp/tablet.tsv"
reject
cp "$tmp/good.tsv" "$tmp/tablet.tsv"
if sh "$selector" "$tmp/tablet.tsv" "$tmp/paired" 2222222222222222222222222222222222222222 > "$tmp/out" 2> "$tmp/err"; then
    echo 'different source revision accepted' >&2; exit 1
fi
sed 's/TAB_P10/MIRO_C67/g' "$tmp/paired/android-plan-tablet.tsv" > "$tmp/changed"
mv "$tmp/changed" "$tmp/paired/android-plan-tablet.tsv"
reject
sed 's/MIRO_C67/TAB_P10/g' "$tmp/paired/android-plan-tablet.tsv" > "$tmp/changed"
mv "$tmp/changed" "$tmp/paired/android-plan-tablet.tsv"
mv "$tmp/paired/fastchat-arm64-v8a.apk" "$tmp/paired/missing.apk"
reject
printf '%s\n' 'PASS TAB_P10 synthetic eligibility / source binding / no-device refusal; NOT a signed or physical APK test'
