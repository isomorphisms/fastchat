#!/usr/bin/env bash
# Fixed interface: the registered FP Android stage invokes recipes with Bash.
# All producer policy and commands remain in the canonical Grease procedure.
set -eu
: "${GREASE_BINARY:?exact Grease runtime must be supplied by the producer}"
exec "$GREASE_BINARY" "${BASH_SOURCE[0]%.sh}.grease" "$@"
