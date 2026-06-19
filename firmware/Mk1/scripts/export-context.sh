#!/usr/bin/env bash
set -euo pipefail

OUT="${1:-pocketsat_context.md}"

PROJECT_NAME="$(basename "$(pwd)")"
NOW="$(date -Iseconds)"

cat > "$OUT" <<EOF
# ${PROJECT_NAME} project snapshot

Generated: ${NOW}

## Project tree

\`\`\`text
EOF

if command -v tree >/dev/null 2>&1; then
    tree -a \
        -I '.git|.pio|build|dist|*.zip|*.elf|*.bin|*.hex|*.o|*.a|*.map|compile_commands.json' \
        >> "$OUT"
else
    find . \
        -path './.git' -prune -o \
        -path './.pio' -prune -o \
        -type f \
        ! -name '*.zip' \
        ! -name '*.elf' \
        ! -name '*.bin' \
        ! -name '*.hex' \
        ! -name '*.o' \
        ! -name '*.a' \
        ! -name '*.map' \
        -print | sort >> "$OUT"
fi

cat >> "$OUT" <<EOF
\`\`\`

## Git status

\`\`\`text
EOF

if command -v git >/dev/null 2>&1 && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git status --short >> "$OUT"
else
    echo "Not a git repository." >> "$OUT"
fi

cat >> "$OUT" <<EOF
\`\`\`

## Source files

EOF

find include src docs -type f 2>/dev/null \
    \( -name '*.h' -o -name '*.c' -o -name '*.md' -o -name '*.ini' -o -name '*.txt' \) \
    | sort \
    | while read -r file; do
        ext="${file##*.}"

        case "$ext" in
            c) lang="c" ;;
            h) lang="c" ;;
            md) lang="markdown" ;;
            ini) lang="ini" ;;
            txt) lang="text" ;;
            *) lang="text" ;;
        esac

        {
            echo "### \`$file\`"
            echo
            echo "\`\`\`$lang"
            cat "$file"
            echo
            echo "\`\`\`"
            echo
        } >> "$OUT"
    done

if [ -f platformio.ini ]; then
    {
        echo "### \`platformio.ini\`"
        echo
        echo "\`\`\`ini"
        cat platformio.ini
        echo
        echo "\`\`\`"
        echo
    } >> "$OUT"
fi

echo "Wrote $OUT"
