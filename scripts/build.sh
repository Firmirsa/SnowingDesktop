#!/usr/bin/env bash
# Pretty build for Snow on Desktop.  Called by `make`, `make run`, `make zip`, `make demo`.
# Raw compiler output goes to build/build.log; `make V=1` bypasses this script.
# Compatible with the bash 3.2 that ships with macOS.

set -u
GOAL="${1:-app}"
MAKE_CMD="${MAKE:-make}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1
mkdir -p build
LOG="build/build.log"
: > "$LOG"

# ---- terminal ---------------------------------------------------------------
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ] && [ "${TERM:-dumb}" != "dumb" ]; then
    FANCY=1
    R=$'\033[0m'; B=$'\033[1m'; D=$'\033[2m'
    C1=$'\033[38;5;195m'; C2=$'\033[38;5;159m'; C3=$'\033[38;5;123m'
    C4=$'\033[38;5;87m';  C5=$'\033[38;5;81m';  C6=$'\033[38;5;75m'
    OK=$'\033[38;5;121m'; ERR=$'\033[38;5;203m'; GREY=$'\033[38;5;244m'; WHITE=$'\033[38;5;231m'
    CLR=$'\r\033[K'
    printf '\033[?25l'                                   # hide cursor
    trap 'printf "\033[?25h"' EXIT
    trap 'printf "\033[?25h\n"; exit 130' INT TERM
else
    FANCY=0
    CLR=""
    R=""; B=""; D=""; C1=""; C2=""; C3=""; C4=""; C5=""; C6=""; OK=""; ERR=""; GREY=""; WHITE=""
fi

now() { perl -MTime::HiRes=time -e 'printf "%.1f", time' 2>/dev/null || date +%s; }
elapsed() { awk -v a="$1" -v b="$(now)" 'BEGIN { printf "%.1fs", b - a }'; }

# ---- banner -----------------------------------------------------------------
BANNER=(
"  ███████╗███╗   ██╗ ██████╗ ██╗    ██╗"
"  ██╔════╝████╗  ██║██╔═══██╗██║    ██║"
"  ███████╗██╔██╗ ██║██║   ██║██║ █╗ ██║"
"  ╚════██║██║╚██╗██║██║   ██║██║███╗██║"
"  ███████║██║ ╚████║╚██████╔╝╚███╔███╔╝"
"  ╚══════╝╚═╝  ╚═══╝ ╚═════╝  ╚══╝╚══╝ "
)
COLORS=("$C1" "$C2" "$C3" "$C4" "$C5" "$C6")
WIDTH=44
FLAKES=("❄" "*" "·" "✻" "·" "*")

snow_row() { # a random row of snowflakes
    local row="" i r
    for ((i = 0; i < WIDTH; i++)); do
        r=$((RANDOM % 14))
        if [ $r -lt 1 ]; then row="$row${WHITE}${FLAKES[$((RANDOM % 6))]}${R}"
        elif [ $r -lt 2 ]; then row="$row${GREY}·${R}"
        else row="$row "; fi
    done
    printf '%s' "$row"
}

banner() {
    echo
    if [ $FANCY -eq 1 ]; then
        local rows=("" "" "") f i
        for ((i = 0; i < 3; i++)); do echo; done
        for ((f = 0; f < 16; f++)); do                      # snow falls above the title
            rows=("$(snow_row)" "${rows[0]}" "${rows[1]}")
            printf '\033[3A'
            for ((i = 0; i < 3; i++)); do printf '\r\033[K  %s\n' "${rows[$i]}"; done
            sleep 0.06
        done
    fi
    for ((i = 0; i < ${#BANNER[@]}; i++)); do
        printf '%s%s%s%s\n' "$B" "${COLORS[$i]}" "${BANNER[$i]}" "$R"
        [ $FANCY -eq 1 ] && sleep 0.03
    done
    printf '  %s%so n   d e s k t o p%s     %sit snows on your Mac%s\n\n' "$B" "$C2" "$R" "$GREY" "$R"
}

# ---- steps ------------------------------------------------------------------
step_done() { # label, time
    printf '%s  %s✓%s %-34s %s%s%s\n' "$CLR" "$OK" "$R" "$1" "$GREY" "$2" "$R"
}
step_cached() {
    printf '  %s✓%s %-34s %s%s%s\n' "$OK" "$R" "$1" "$GREY" "cached" "$R"
}

fail() { # label
    printf '%s  %s✗ %s%s\n\n' "$CLR" "$ERR" "$1" "$R"
    printf '%s  ── last lines of %s ──%s\n' "$GREY" "$LOG" "$R"
    tail -n 25 "$LOG" | sed 's/^/  /'
    printf '\n  %sFull log: %s   ·   Raw build: make V=1%s\n\n' "$GREY" "$LOG" "$R"
    exit 1
}

bar() { # percent, label, detail
    local pct=$1 n=$(( $1 * 24 / 100 )) full="" empty="" i
    for ((i = 0; i < n; i++)); do full="${full}█"; done
    for ((i = n; i < 24; i++)); do empty="${empty}░"; done
    if [ $FANCY -eq 1 ]; then
        printf '\r\033[K  %s❄%s %-24s %s%s%s%s%s %3d%%  %s%s%s' \
            "$C3" "$R" "$2" "$C4" "$full" "$R" "$GREY" "$empty" "$pct" "$GREY" "$3" "$R"
    fi
}

# run a make target with a spinner
spin() { # label, done label, make target...
    local label=$1 done_label=$2; shift 2
    local t0; t0=$(now)
    "$MAKE_CMD" --no-print-directory "$@" >> "$LOG" 2>&1 &
    local pid=$! i=0
    local frames=("⠋" "⠙" "⠹" "⠸" "⠼" "⠴" "⠦" "⠧" "⠇" "⠏")
    while kill -0 $pid 2>/dev/null; do
        [ $FANCY -eq 1 ] && printf '\r\033[K  %s%s%s %s%s%s' "$C3" "${frames[$((i % 10))]}" "$R" "$label" "$GREY" "…$R"
        i=$((i + 1)); sleep 0.08
    done
    wait $pid || fail "$label"
    step_done "$done_label" "$(elapsed "$t0")"
}

download_raylib() {
    local label="Downloading raylib 5.5" t0; t0=$(now)
    bar 0 "$label" ""
    set -o pipefail
    "$MAKE_CMD" --no-print-directory raw-raylib-src 2>&1 | tr '\r' '\n' | while IFS= read -r line; do
        echo "$line" >> "$LOG"
        case "$line" in
            *"Receiving objects:"*)
                p=${line#*Receiving objects:}; p=${p%%\%*}; p=${p// /}
                case "$p" in ''|*[!0-9]*) ;; *) bar $(( p * 9 / 10 )) "$label" "receiving";; esac ;;
            *"Resolving deltas:"*) bar 95 "$label" "resolving" ;;
        esac
    done || fail "$label"
    set +o pipefail
    step_done "raylib 5.5 downloaded" "$(elapsed "$t0")"
}

compile_raylib() {
    local label="Compiling raylib" done_label="raylib compiled" t0 total=8 n=0; t0=$(now)
    case "$(uname -s)" in Darwin) label="Compiling raylib (universal)"; done_label="raylib compiled (universal)";; esac
    bar 0 "$label" ""
    set -o pipefail
    "$MAKE_CMD" --no-print-directory raw-raylib 2>&1 | while IFS= read -r line; do
        echo "$line" >> "$LOG"
        case "$line" in
            *" -c "*.c*)
                f=""
                for w in $line; do case "$w" in *.c) f=$w; break;; esac; done
                n=$((n + 1)); [ $n -gt $total ] && n=$total
                bar $(( n * 100 / (total + 1) )) "$label" "$f" ;;
            *"ar rcs"*|*"libraylib.a"*) bar 99 "$label" "linking" ;;
        esac
    done || fail "$label"
    set +o pipefail
    step_done "$done_label" "$(elapsed "$t0")"
}

# ---- main -------------------------------------------------------------------
START=$(now)
banner

if [ -f vendor/raylib/src/raylib.h ]; then step_cached "raylib 5.5 downloaded"; else download_raylib; fi
if [ -f vendor/raylib/src/libraylib.a ]; then step_cached "raylib compiled"; else compile_raylib; fi

case "$GOAL" in
    demo)
        if "$MAKE_CMD" --no-print-directory -q build/snow-demo >/dev/null 2>&1; then step_cached "Demo compiled"
        else spin "Compiling the demo" "Demo compiled" build/snow-demo; fi
        RESULT="build/snow-demo"; NEXT="./build/snow-demo" ;;
    *)
        if "$MAKE_CMD" --no-print-directory -q build/SnowOnDesktop >/dev/null 2>&1; then step_cached "Snow on Desktop compiled"
        else spin "Compiling Snow on Desktop" "Snow on Desktop compiled" build/SnowOnDesktop; fi
        spin "Packaging & signing the app" "App packaged & signed" raw-app
        RESULT="build/Snow on Desktop.app"; NEXT="make run"
        if [ "$GOAL" = "zip" ]; then
            spin "Zipping for release" "Release zip ready" raw-zip
            RESULT="$(ls build/SnowOnDesktop-*-macOS.zip 2>/dev/null | head -n 1)"
            NEXT="upload it to a GitHub release"
        fi ;;
esac

# ---- done -------------------------------------------------------------------
TOTAL=$(elapsed "$START")
line="──────────────────────────────────────────────"
echo
printf '  %s╭%s%s\n' "$C4" "$line" "$R"
printf '  %s│%s  %s%s❄  Ready!%s %sbuilt in %s%s\n' "$C4" "$R" "$B" "$WHITE" "$R" "$GREY" "$TOTAL" "$R"
printf '  %s│%s     %s%s%s\n' "$C4" "$R" "$C2" "$RESULT" "$R"
if [ "$GOAL" != "run" ]; then
    printf '  %s│%s     %snext:%s %s\n' "$C4" "$R" "$GREY" "$R" "$NEXT"
fi
printf '  %s╰%s%s\n\n' "$C4" "$line" "$R"

if [ "$GOAL" = "run" ]; then
    printf '  %s❄%s Launching… look for the snowflake in your menu bar %s❄%s\n\n' "$C3" "$R" "$C3" "$R"
    "$MAKE_CMD" --no-print-directory raw-run >> "$LOG" 2>&1 || fail "Launching"
fi
