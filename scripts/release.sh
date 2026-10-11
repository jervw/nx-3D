#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
if [[ $# -ne 1 || ! "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Usage: $0 VERSION (for example, 0.1.1)" >&2
    exit 2
fi
version="$1"
command -v gh >/dev/null || { echo 'GitHub CLI (gh) is required' >&2; exit 1; }
[[ -z "$(git -C "$repo_root" status --porcelain --untracked-files=normal)" ]] || {
    echo 'Commit your changes before releasing' >&2
    exit 1
}
branch="$(git -C "$repo_root" branch --show-current)"
default_branch="$(gh repo view --json defaultBranchRef --jq '.defaultBranchRef.name')"
[[ "$branch" == "$default_branch" ]] || { echo "Switch to $default_branch before releasing" >&2; exit 1; }
git -C "$repo_root" fetch origin "$default_branch" --quiet
[[ "$(git -C "$repo_root" rev-parse HEAD)" == "$(git -C "$repo_root" rev-parse "origin/$default_branch")" ]] || {
    echo "Push the current $default_branch commit before releasing" >&2
    exit 1
}
if git -C "$repo_root" ls-remote --exit-code --tags origin "refs/tags/v$version" >/dev/null; then
    echo "Tag v$version already exists" >&2
    exit 1
fi
gh workflow run manual-release.yml --ref "$default_branch" -f "version=$version"
echo "Release v$version started. Follow it with: gh run list --workflow manual-release.yml"
