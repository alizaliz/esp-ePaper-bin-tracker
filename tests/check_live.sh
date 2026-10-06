#!/usr/bin/env bash
# Checks the live Auckland Council collection day page still parses, from
# your own connection. Run from the repo root:
#   tests/check_live.sh <assessment number>
# Without an argument, it uses COUNCIL_ADDRESS_ID from src/config.h if set.
#
# The Council website refuses requests from VPNs and data centres (HTTP 406),
# which is why this runs locally rather than on GitHub's servers.
set -euo pipefail

NUMBER="${1:-}"
if [ -z "$NUMBER" ] && [ -f src/config.h ]; then
  NUMBER=$(sed -nE 's/^#define COUNCIL_ADDRESS_ID "([0-9]*)".*/\1/p' src/config.h)
fi
if [ -z "$NUMBER" ]; then
  echo "Usage: tests/check_live.sh <assessment number>" >&2
  exit 2
fi

PAGE=$(mktemp)
trap 'rm -f "$PAGE"' EXIT
URL="https://www.aucklandcouncil.govt.nz/en/rubbish-recycling/rubbish-recycling-collections/rubbish-recycling-collection-days/${NUMBER}.html"
STATUS=$(curl -s --max-time 30 -A "esp-ePaper-bin-tracker" -o "$PAGE" -w '%{http_code}' "$URL" || echo "000")

case "$STATUS" in
  200) ;;
  404) echo "Page not found (HTTP 404): check the assessment number." >&2; exit 1 ;;
  406|403) echo "The Council website refused the request (HTTP $STATUS). It blocks VPNs and data centres: turn off any VPN and try again." >&2; exit 1 ;;
  *) echo "Couldn't fetch the page (HTTP $STATUS)." >&2; exit 1 ;;
esac

TZ=Pacific/Auckland tests/run.sh --live "$PAGE"
