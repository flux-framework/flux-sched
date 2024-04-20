#!/usr/bin/env bash
# first version pulled from commit 3f0542e on April 19
# add command: git subtree add --prefix=cmake/sanitizers-cmake --squash https://github.com/arsenm/sanitizers-cmake master
#
# To rebase across an update, use `git rebase --rebase-merges --strategy subtree
# if the rebase stops and says "refusing to merge unrelated histories" then use
# the add command above or pull or similar to re-create the merge commit
git subtree pull --prefix cmake/sanitizers-cmake https://github.com/arsenm/sanitizers-cmake master --squash
