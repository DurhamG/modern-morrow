#!/bin/bash
# Tests CI/linux/appimage/AppRun, the AppImage's start script, which runs the setup wizard before the game until the
# game is set up.
#
# Each test copies AppRun into a folder of its own, beside stand-in openmw, openmw-wizard and openmw-launcher programs
# that write their name and arguments to a file. The stand-in wizard writes the settings that FAKE_WIZARD_WRITES
# names, as the real wizard would. XDG_CONFIG_HOME points at the test's folder, so the tests never read the real
# settings.
#
# Run it with bash CI/test_apprun.sh. It prints ok or FAILED for each test, and exits with 1 when any failed.
set -u

APPRUN="$(cd "$(dirname "$0")" && pwd)/linux/appimage/AppRun"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
failures=0

setup() {
    T="$WORK/$1"
    mkdir -p "$T/config/modern-morrow"
    cp "$APPRUN" "$T/AppRun"
    for program in openmw openmw-launcher; do
        printf '#!/bin/bash\necho "%s $*" >> "%s/runs"\n' "$program" "$T" > "$T/$program"
    done
    # The wizard writes the data folder, or the data folder and a game file, or nothing when it is cancelled.
    cat > "$T/openmw-wizard" << EOF
#!/bin/bash
echo "openmw-wizard \$*" >> "$T/runs"
case "\${FAKE_WIZARD_WRITES:-}" in
    data) echo 'data="/games/Morrowind/Data Files"' >> "$T/config/modern-morrow/openmw.cfg" ;;
    content)
        echo 'data="/games/Morrowind/Data Files"' >> "$T/config/modern-morrow/openmw.cfg"
        echo 'content=Morrowind.esm' >> "$T/config/modern-morrow/openmw.cfg"
        ;;
esac
EOF
    chmod +x "$T/AppRun" "$T/openmw" "$T/openmw-launcher" "$T/openmw-wizard"
    CONFIG="$T/config/modern-morrow/openmw.cfg"
    export FAKE_WIZARD_WRITES=""
}

run_apprun() {
    XDG_CONFIG_HOME="$T/config" sh "$T/AppRun" "$@" > "$T/stdout" 2> "$T/stderr"
    STATUS=$?
}

ran() { tr '\n' '|' < "$T/runs" 2> /dev/null; }

check() {
    local name="$1"
    shift
    if "$@"; then
        echo "ok $name"
    else
        echo "FAILED $name"
        echo "  runs: $(ran)"
        echo "  status: $STATUS, stderr: $(cat "$T/stderr")"
        failures=$((failures + 1))
    fi
}

no_config_runs_the_wizard_then_the_game() {
    setup no-config
    FAKE_WIZARD_WRITES=content run_apprun
    [[ "$(ran)" == "openmw-wizard |openmw |" ]]
}

config_with_a_game_file_starts_the_game() {
    setup configured
    printf 'data="/games/Morrowind/Data Files"\ncontent=Morrowind.esm\n' > "$CONFIG"
    run_apprun
    [[ "$(ran)" == "openmw |" ]]
}

omwgame_counts_as_a_game_file() {
    setup omwgame
    printf 'content="template.omwgame"\n' > "$CONFIG"
    run_apprun
    [[ "$(ran)" == "openmw |" ]]
}

plugin_alone_is_not_a_game_file() {
    setup plugin-only
    printf 'data="/games/Morrowind/Data Files"\ncontent=Tribunal.esp\n' > "$CONFIG"
    FAKE_WIZARD_WRITES=content run_apprun
    [[ "$(ran)" == "openmw-wizard |openmw |" ]]
}

wizard_cancelled_does_not_start_the_game() {
    setup cancelled
    run_apprun
    [[ "$(ran)" == "openmw-wizard |" && "$STATUS" == 1 ]]
}

data_folder_without_game_file_opens_the_launcher() {
    setup data-only
    FAKE_WIZARD_WRITES=data run_apprun
    [[ "$(ran)" == "openmw-wizard |openmw-launcher |" ]]
}

launcher_option_opens_the_launcher() {
    setup launcher
    run_apprun --launcher
    [[ "$(ran)" == "openmw-launcher |" ]]
}

wizard_option_runs_the_wizard() {
    setup wizard
    printf 'content=Morrowind.esm\n' > "$CONFIG"
    run_apprun --wizard
    [[ "$(ran)" == "openmw-wizard |" ]]
}

game_option_starts_the_game() {
    setup game
    run_apprun --game --skip-menu
    [[ "$(ran)" == "openmw --skip-menu|" ]]
}

other_arguments_reach_the_game() {
    setup arguments
    printf 'content=Morrowind.esm\n' > "$CONFIG"
    run_apprun --skip-menu --new-game
    [[ "$(ran)" == "openmw --skip-menu --new-game|" ]]
}

for test in no_config_runs_the_wizard_then_the_game config_with_a_game_file_starts_the_game \
    omwgame_counts_as_a_game_file plugin_alone_is_not_a_game_file wizard_cancelled_does_not_start_the_game \
    data_folder_without_game_file_opens_the_launcher launcher_option_opens_the_launcher \
    wizard_option_runs_the_wizard game_option_starts_the_game other_arguments_reach_the_game; do
    check "$test" "$test"
done

if ((failures)); then
    echo "$failures test(s) failed."
    exit 1
fi
echo "All AppRun tests passed."
