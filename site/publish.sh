#!/bin/sh
# builds the gh-pages branch from site/ + install.sh, no github actions needed.
#   sh site/publish.sh
# it only makes the local branch. it prints the push command, run that yourself.
# every run is a fresh one-commit branch, so the push has to be forced.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
rev=$(git -C "$root" rev-parse --short HEAD)
[ -z "$(git -C "$root" status --porcelain -- site install.sh ':!site/publish.sh')" ] || echo "note: site/ has changes that are not committed, they go in too"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

cp -r "$root/site/." "$tmp/"
rm -rf "$tmp/tools" "$tmp/publish.sh"
cp "$root/install.sh" "$tmp/"
touch "$tmp/.nojekyll"
[ -f "$tmp/CNAME" ] || echo tinyclawd.deepakpopli.in > "$tmp/CNAME"

cd "$tmp"
git init -q
git checkout -q -b gh-pages
git config user.name "$(git -C "$root" config user.name)"
git config user.email "$(git -C "$root" config user.email)"
git add -A
git commit -q -m "site:from $rev"
git -C "$root" fetch -q "$tmp" +gh-pages:gh-pages

echo "gh-pages is ready ($(git -C "$root" ls-tree -r --name-only gh-pages | wc -l) files, from $rev)"
echo "push it with:"
echo "    git push -f origin gh-pages"
