#!/bin/sh
# Read-only candidate selection, not APK signature validation or installation.
# Cat Food inventories are observations, not cryptographic attestation.
set -eu
fail() { printf 'FastChat TAB_P10 selector: %s\n' "$*" >&2; exit 2; }
[ "$#" -eq 3 ] || fail 'usage: select-tablet-apk.sh INVENTORY PAIRED_OUTPUT EXPECTED_SOURCE_SHA'
inventory=$1
bundle=$2
source=$3
[ -f "$inventory" ] || fail 'device inventory unavailable'
case "$bundle" in /*) ;; *) fail 'paired output path must be absolute' ;; esac
printf '%s\n' "$source" | grep -Eq '^[0-9a-f]{40}$' || fail 'source must be an exact commit'
awk -F '\t' '
    NR==1 {if($0!="# catfood-device-inventory-v1") bad=1}
    $1=="meta" {
        if($2=="target" || $2=="model" || $2=="product" ||
           $2=="abi" || $2=="android_sdk" || $2=="device_id" ||
           $2=="observed_at") {
            count[$2]++; value[$2]=$3; evidence[$2]=$4
        }
    }
    END {
        split("target model product abi android_sdk device_id observed_at", keys, " ")
        for(i=1;i<=7;i++) if(count[keys[i]]!=1) bad=1
        if(value["target"]!="tablet" || evidence["target"]!="detected") bad=1
        if(value["abi"]!="arm64-v8a" || evidence["abi"]!="getprop") bad=1
        if(value["android_sdk"] !~ /^[0-9]+$/ || value["android_sdk"]+0 < 26 ||
           evidence["android_sdk"]!="getprop") bad=1
        if(!(value["model"]=="TAB_P10" || value["model"]=="TAB_P10_ROW" ||
             value["product"]=="TAB_P10" || value["product"]=="TAB_P10_ROW")) bad=1
        if(value["device_id"] !~ /^[A-Za-z0-9_.-]+$/ ||
           value["observed_at"]=="" || value["observed_at"]=="-") bad=1
        exit bad
    }
' "$inventory" || fail 'tablet identity/ABI/SDK is missing, inconsistent or unobserved'
plan="$bundle/android-plan-tablet.tsv"
binding="$bundle/arm64-v8a/native/source-binding.tsv"
apk="$bundle/fastchat-arm64-v8a.apk"
[ -f "$plan" ] && [ -f "$binding" ] || fail 'tablet plan/source binding unavailable'
awk -F '\t' -v expected="$source" '
    $1=="schema" {n["schema"]++; if($2!="catfood-android-build-plan-v1") bad=1}
    $1=="source_sha" {n["source"]++; if($2!=expected) bad=1}
    $1=="requested_target" {n["requested"]++; if($2!="tablet") bad=1}
    $1=="package_id" {n["package"]++; if($2!="org.isomorphisms.fastchat.diskstreaming.arena") bad=1}
    $1=="tablet" {n["tablet"]++; if($2!="TAB_P10" || $3!="arm64-v8a" ||
                                     $4!="required" || $5!="not_run") bad=1}
    $1=="phone" || $1=="c67" {bad=1}
    END {
        split("schema source requested package tablet", keys, " ")
        for(i=1;i<=5;i++) if(n[keys[i]]!=1) bad=1
        exit bad
    }
' "$plan" || fail 'tablet plan source/target differs from expected'
awk -F '\t' -v expected="$source" '
    $1=="source_commit" {n++; if($2!=expected) bad=1}
    END {exit bad || n!=1}
' "$binding" || fail 'native source binding differs from expected revision'
[ -s "$apk" ] || fail 'ARM64 candidate unavailable'
printf 'status\tELIGIBLE_ONLY\ntarget\ttablet\nabi\tarm64-v8a\nsource_commit\t%s\napk\t%s\napk_sha256\t%s\nsigner\tNOT_VERIFIED\nphysical_install\tNOT_RUN\n' \
    "$source" "$apk" "$(sha256sum "$apk" | awk '{print $1}')"
