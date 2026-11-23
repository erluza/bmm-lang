#!/bin/sh
set -e
cd "$(dirname "$0")/.."

FAIL=0
mkdir -p /tmp/opencode
ALLOWED="stdint.h stddef.h stdarg.h limits.h string.h"

# 1. headers outside allowed list in non-pal files (gc.c may use stdlib.h)
for f in $(find src -type f \( -name '*.c' -o -name '*.h' \) | grep -v 'src/pal/'); do
    if echo "$f" | grep -q 'src/gc.c'; then continue; fi
    grep -oE '#include[[:space:]]*<[A-Za-z0-9_./-]+>' "$f" | sed -E 's/.*<([^>]+)>/\1/' | while read -r h; do
        base=$(basename "$h")
        case " $ALLOWED " in
            *" $base "*) ;;
            *) echo "DISALLOWED HEADER in $f: $h"; exit 1 ;;
        esac
    done || FAIL=1
done

# 2. forbidden tokens outside src/pal (except gc.c)
FORBIDDEN='\\bFILE\\b|\\bstdin\\b|\\bstdout\\b|\\bstderr\\b|\\bprintf\\b|\\bfprintf\\b|\\bsnprintf\\b|\\bsprintf\\b|\\bputs\\b|\\bfputs\\b|\\bfputc\\b|\\bfgets\\b|\\bfopen\\b|\\bfread\\b|\\bfwrite\\b|\\bfclose\\b|\\bfflush\\b|\\bgetchar\\b|\\babort\\b|\\bmalloc\\b|\\bcalloc\\b|\\brealloc\\b|\\bfree\\b'
for f in $(find src -type f \( -name '*.c' -o -name '*.h' \) | grep -v 'src/pal/'); do
    if echo "$f" | grep -q 'src/gc.c'; then continue; fi
    if ! bad=$(grep -nE "$FORBIDDEN" "$f" | grep -v '"exit"' | grep -v '^[0-9]*:[[:space:]]*//' | grep -v '^[0-9]*:[[:space:]]*\\*'); then bad=""; fi
    if [ -n "$bad" ]; then
        echo "FORBIDDEN TOKEN in $f:"
        echo "$bad"
        FAIL=1
    fi
done

# 3. pal_template compiles
cc -std=c99 -Wall -Wextra -Wpedantic -Werror -Isrc -Isrc/pal -c src/pal/pal_template.c -o /tmp/opencode/pal_template.o || FAIL=1

# 4. freestanding compile of core (without -nostdinc to locate headers)
GCCINC=$(cc -print-file-name=include)
if [ -d "$GCCINC" ]; then
    cc -ffreestanding -Isrc -Isrc/pal -I"$GCCINC" -I/usr/include -std=c99 -Wall -Wextra -Wpedantic -Werror \
      -c src/lexer.c -o /tmp/opencode/lexer_fs.o 2>/tmp/opencode/fs.log || { cat /tmp/opencode/fs.log; FAIL=1; }
fi

if [ "$FAIL" -ne 0 ]; then
    echo "portability_check FAILED"
    exit 1
fi
echo "portability_check PASSED"
exit 0
