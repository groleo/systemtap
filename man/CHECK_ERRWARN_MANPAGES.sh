#! /bin/sh

# Run this in source tree, to check whether all error/warning man pages mentioned
# in the systemtap sources actually exist.
set -e

# NB: we don't want this regexp to match itself
(cd ..; git grep -E '\[man (error|warning)::.*\]' | cut -f2 -d[ | cut -f1 -d] | cut -f2 -d' ') | while read manpage
do
    file="$manpage".7stap
    if [ -f $file ]; then
        echo "ok $file"
    else
        echo "KO $file"
    fi
done
