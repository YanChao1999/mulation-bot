#!/bin/sh
# Post GitHub PR review comments for survived mutants from a mulation --json-out file.
#
# Usage (in a pull_request workflow after mulation --json-out report.json):
#   sh scripts/pr-comment-survivors.sh report.json
#
# Requires: gh, jq, GITHUB_TOKEN (or gh auth), and a PR context
#   (GITHUB_EVENT_PATH / gh pr view, or MULATION_PR_NUMBER).
#
# Environment:
#   MULATION_PR_NUMBER   Override PR number
#   MULATION_REPO        owner/name (default: from gh)
#   MULATION_MAX_COMMENTS  Cap comments per run (default: 20)
#   MULATION_DRY_RUN=1   Print planned comments; do not call the API
set -eu

json=${1:?usage: pr-comment-survivors.sh <mulation-report.json>}
[ -f "$json" ] || {
    echo "missing report: $json" >&2
    exit 1
}

command -v jq >/dev/null || {
    echo "jq required" >&2
    exit 1
}

DRY=${MULATION_DRY_RUN:-0}
MAX=${MULATION_MAX_COMMENTS:-20}
REPO=${MULATION_REPO:-}
PR=${MULATION_PR_NUMBER:-}
HEAD_SHA=

if [ "$DRY" != "1" ]; then
    command -v gh >/dev/null || {
        echo "gh required (or set MULATION_DRY_RUN=1)" >&2
        exit 1
    }
    if [ -z "$REPO" ]; then
        REPO=$(gh repo view --json nameWithOwner -q .nameWithOwner 2>/dev/null || true)
    fi
    [ -n "$REPO" ] || {
        echo "set MULATION_REPO=owner/name" >&2
        exit 1
    }
    if [ -z "$PR" ] && [ -n "${GITHUB_EVENT_PATH:-}" ] && [ -f "$GITHUB_EVENT_PATH" ]; then
        PR=$(jq -r '.pull_request.number // .number // empty' "$GITHUB_EVENT_PATH" 2>/dev/null || true)
    fi
    if [ -z "$PR" ]; then
        PR=$(gh pr view --json number -q .number 2>/dev/null || true)
    fi
    [ -n "$PR" ] && [ "$PR" != "null" ] || {
        echo "no pull request context (set MULATION_PR_NUMBER)" >&2
        exit 1
    }
    HEAD_SHA=$(gh api "repos/${REPO}/pulls/${PR}" --jq .head.sha)
else
    REPO=${REPO:-dry-run/local}
    PR=${PR:-0}
    echo "mulation pr-bot: DRY RUN (no API calls)"
fi

n=$(jq '.survivors | length' "$json")
echo "mulation pr-bot: ${n} survivor(s), commenting up to ${MAX} on PR #${PR}"

# Deduplicate by file:line so one line gets one comment summarizing ops.
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT
jq -r '
  .survivors
  | group_by(.file + ":" + (.line|tostring))
  | .[]
  | {
      file: .[0].file,
      line: .[0].line,
      kinds: ([.[].kind] | unique | join(",")),
      ops: ([.[] | "\(.op) -> \(.mut)"] | unique | join("; "))
    }
  | "\(.file)\t\(.line)\t\(.kinds)\t\(.ops)"
' "$json" >"$tmp"

count=0
while IFS="$(printf '\t')" read -r file line kinds ops; do
    [ -n "$file" ] || continue
    count=$((count + 1))
    if [ "$count" -gt "$MAX" ]; then
        echo "hit MULATION_MAX_COMMENTS=${MAX}; stopping"
        break
    fi

    # Prefer repo-relative paths for GitHub review comments.
    rel=$file
    case "$file" in
    */*)
        # strip leading workspace abs paths if present
        rel=$(printf '%s' "$file" | sed -e 's|.*/mulation-bot/||' -e 's|^/||')
        ;;
    esac

    body=$(
        cat <<EOF
**mulation:** mutant(s) survived here (\`${kinds}\`)

\`$ops\`

Coverage may have hit this line, but no assertion depended on the correct operator.
Strengthen a boundary/contract check (e.g. ages 17 vs 18 for \`>=\` vs \`>\`), then re-run \`mulation --git-diff\`.
EOF
    )

    if [ "$DRY" = "1" ]; then
        echo "dry-run would comment ${rel}:${line} [$kinds] $ops"
        continue
    fi

    # Create a pending review comment via the pulls API (single-line).
    # Ignore failures for paths outside the PR diff (GitHub rejects those).
    if ! gh api "repos/${REPO}/pulls/${PR}/comments" \
        -f body="$body" \
        -f commit_id="$HEAD_SHA" \
        -f path="$rel" \
        -F line="$line" \
        -f side=RIGHT >/dev/null 2>&1; then
        echo "skip (not in diff or API error): ${rel}:${line}"
    else
        echo "commented ${rel}:${line}"
    fi
done <"$tmp"

echo "mulation pr-bot: done"
