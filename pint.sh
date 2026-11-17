#!/bin/sh
# pint.sh - the games for PINT, Windows NT on the Raspberry Pi (ARM), built with the PINT SDKs in
# Docker: tenox7/pint35sdk for PINT 3.5, tenox7/pint40sdk for PINT 4.0 (their pages tell how to set
# them up once, in the Docker volume msvc-arm32).
# Usage: ./pint.sh [35] [40]   (default: both)
# Output: out/pint35/ and out/pint40/ with the 11 exes, and out/ntgames-pint35.zip, ntgames-pint40.zip.
# MSVC=<volume or folder> and SDKTAG=<image tag> choose another Microsoft C or SDK image.

cd "$(dirname "$0")" || exit 1
ROOT=$(pwd)
[ $# -gt 0 ] || set -- 35 40
# folder:exe of each game
GAMES="2048:2048 breakout:breakout bubbles:bubbles lander:lander missile:misl ntrigue:ntrigue
       riscoban:RISCoban riscque:riscque snake:snake sokoban:sokoban tankz:tankz"

# levels DIR: DIR/levels.h and DIR/levels.rc for the levels in DIR/levels, as genlevels.exe makes them
# (genlevels.exe would be an ARM exe here, which the computer that builds cannot run)
levels() {
    n=0
    printf '/* Auto-generated level resources */\r\n' > "$1/levels.rc"
    hdr=
    for f in $(cd "$1" && ls levels/*.sok | LC_ALL=C sort); do
        printf '%d RCDATA "levels\\\\%s"\r\n' $((3000 + n)) "${f#levels/}" >> "$1/levels.rc"
        hdr="$hdr#define IDR_LEVEL_$n $((3000 + n))\r\n"
        n=$((n + 1))
    done
    printf "/* Auto-generated level resource IDs */\r\n#ifndef LEVELS_H\r\n#define LEVELS_H\r\n\r\n#define IDR_LEVEL_BASE 3000\r\n#define NUM_LEVELS $n\r\n\r\n$hdr\r\n#endif /* LEVELS_H */\r\n" > "$1/levels.h"
}

rc=0
for v in "$@"; do
    case $v in 35|40) ;; *) echo "pint.sh: 35 or 40, not $v" >&2; exit 2 ;; esac
    work=$ROOT/.build/pint$v
    out=$ROOT/out/pint$v
    mkdir -p "$work" "$out" && find "$work" "$out" -mindepth 1 -delete || exit 1
    cp arch.h "$work/" || exit 1
    for g in $GAMES; do
        d=${g%%:*}
        mkdir -p "$work/$d" && (cd "$d" && find . -maxdepth 1 -type f ! -name '*.exe' -exec cp {} "$work/$d/" \;) || exit 1
        [ ! -d "$d/levels" ] || cp -R "$d/levels" "$work/$d/" || exit 1
    done
    levels "$work/sokoban" && levels "$work/riscoban" || exit 1
    for g in $GAMES; do
        d=${g%%:*} x=${g#*:}
        echo "=== PINT $v: $d"
        docker run --rm --network none -v "${MSVC:-msvc-arm32}:/opt/msvc:ro" -v "$work:/src" -w "/src/$d" \
            "tenox7/pint${v}sdk${SDKTAG:+:$SDKTAG}" nmake -nologo all && cp "$work/$d/$x.exe" "$out/" || rc=1
    done
    rm -f "$ROOT/out/ntgames-pint$v.zip"
    (cd "$out" && zip -q -X "$ROOT/out/ntgames-pint$v.zip" *.exe) || rc=1
done
exit $rc
