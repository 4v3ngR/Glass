#!/usr/bin/env bash
set -euo pipefail

SRC_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
BUILD_DIR="$SRC_DIR/build"
QT_BUILD=${1:-default}
JOBS=${JOBS:-2}

if (( EUID == 0 )) && [[ ${1:-} != remove ]]; then
    printf 'Run install.sh as your desktop user, without sudo; system steps request sudo themselves.\n' >&2
    exit 1
fi
if (( $# > 1 )); then
    printf 'Usage: %s [QT5|QT6|helper|remove]\n' "$0" >&2
    exit 2
fi

CMAKE_OPTS=(
    -B "$BUILD_DIR"
    -S "$SRC_DIR"
    -DBUILD_TESTING=OFF
    -Wno-dev
    -DKDE_INSTALL_USE_QT_SYS_PATHS=ON
)
PROJECT="glass"
_PROJECT="Glass"
OLD_PROJECT="lightly"
_OLD_PROJECT="Lightly"

build_glass() {
    [[ $JOBS =~ ^[1-9][0-9]*$ ]] || { printf 'JOBS must be a positive integer.\n' >&2; exit 2; }
    if [[ -d $BUILD_DIR ]]; then
        printf 'Removing existing build directory\n'
        sudo rm -rf -- "$BUILD_DIR"
    fi
    case $QT_BUILD in
        qt5|QT5) remove_qt5_files ;;
        qt6|QT6) remove_qt6_files ;;
        default) remove_qt5_files; remove_qt6_files ;;
    esac
    # Each failure exits immediately, so no helper is installed after a failed build.
    cmake "${CMAKE_OPTS[@]}" "$@"
    cmake --build "$BUILD_DIR" -j "$JOBS"
    sudo cmake --install "$BUILD_DIR"
    "$SRC_DIR/tools/glass-user-helper" install
    printf 'Glass installation completed (see any deferred user-service instructions above).\n'
}

# if existing
remove_qt6_files() {
    local f
    local files=(
        "/usr/lib64/qt6/plugins/styles/${PROJECT}6.so*"
        "/usr/lib/qt6/plugins/styles/${PROJECT}6.so*"
        "/usr/share/kstyle/themes/${PROJECT}.themerc"
        "/usr/lib64/qt6/plugins/kstyle_config/${PROJECT}styleconfig.so*"
        "/usr/lib/qt6/plugins/kstyle_config/${PROJECT}styleconfig.so*"
        "/usr/share/applications/${PROJECT}styleconfig.desktop"
        "/usr/bin/${PROJECT}-settings6"
        "/usr/share/icons/hicolor/scalable/apps/${PROJECT}-settings.svgz"
        "/usr/lib64/lib${PROJECT}common6.so*"
        "/usr/lib/lib${PROJECT}common6.so.*"
        "/usr/lib64/lib${PROJECT}common6.so*"
        "/usr/lib/lib${PROJECT}common6.so*"
        "/usr/lib64/qt6/plugins/org.kde.kdecoration3/org.kde.${PROJECT}.so*"
        "/usr/lib/qt6/plugins/org.kde.kdecoration3/org.kde.${PROJECT}.so*"
        "/usr/lib64/qt6/plugins/org.kde.kdecoration3.kcm/kcm_${PROJECT}decoration.so*"
        "/usr/lib/qt6/plugins/org.kde.kdecoration3.kcm/kcm_${PROJECT}decoration.so*"
        "/usr/share/applications/kcm_${PROJECT}decoration.desktop"
        "/usr/lib64/cmake/${PROJECT}/${PROJECT}Config.cmake"
        "/usr/lib/cmake/${PROJECT}/${PROJECT}Config.cmake"
        "/usr/lib64/cmake/${PROJECT}/${PROJECT}ConfigVersion.cmake"
        "/usr/lib/cmake/${PROJECT}/${PROJECT}ConfigVersion.cmake"
        "/usr/share/color-schemes/${_PROJECT}.colors"
        /usr/lib/cmake/"${PROJECT^}"
        "/usr/lib/x86_64-linux-gnu/qt6/plugins/org.kde.kdecoration3/org.kde.${PROJECT}.so*"
        "/usr/lib/x86_64-linux-gnu/qt6/plugins/kstyle_config/${PROJECT}styleconfig.so*"
        "/usr/lib/x86_64-linux-gnu/qt6/plugins/org.kde.kdecoration3.kcm/kcm_${PROJECT}decoration.so*"
        "/usr/lib/x86_64-linux-gnu/qt6/plugins/styles/${PROJECT}6.so*"
        "/usr/lib64/qt6/plugins/styles/${OLD_PROJECT}6.so*"
        "/usr/lib/qt6/plugins/styles/${OLD_PROJECT}6.so*"
        "/usr/share/kstyle/themes/${OLD_PROJECT}.themerc"
        "/usr/lib64/qt6/plugins/kstyle_config/${OLD_PROJECT}styleconfig.so*"
        "/usr/lib/qt6/plugins/kstyle_config/${OLD_PROJECT}styleconfig.so*"
        "/usr/share/applications/${OLD_PROJECT}styleconfig.desktop"
        "/usr/bin/${OLD_PROJECT}-settings6"
        "/usr/share/icons/hicolor/scalable/apps/${OLD_PROJECT}-settings.svgz"
        "/usr/lib64/lib${OLD_PROJECT}common6.so*"
        "/usr/lib/lib${OLD_PROJECT}common6.so.*"
        "/usr/lib64/lib${OLD_PROJECT}common6.so*"
        "/usr/lib/lib${OLD_PROJECT}common6.so*"
        "/usr/lib64/qt6/plugins/org.kde.kdecoration3/org.kde.${OLD_PROJECT}.so*"
        "/usr/lib/qt6/plugins/org.kde.kdecoration3/org.kde.${OLD_PROJECT}.so*"
        "/usr/lib64/qt6/plugins/org.kde.kdecoration3.kcm/kcm_${OLD_PROJECT}decoration.so*"
        "/usr/lib/qt6/plugins/org.kde.kdecoration3.kcm/kcm_${OLD_PROJECT}decoration.so*"
        "/usr/share/applications/kcm_${OLD_PROJECT}decoration.desktop"
        "/usr/lib64/cmake/${OLD_PROJECT}/${OLD_PROJECT}Config.cmake"
        "/usr/lib/cmake/${OLD_PROJECT}/${OLD_PROJECT}Config.cmake"
        "/usr/lib64/cmake/${PROJECT}/${OLD_PROJECT}ConfigVersion.cmake"
        "/usr/lib/cmake/${OLD_PROJECT}/${OLD_PROJECT}ConfigVersion.cmake"
        "/usr/share/color-schemes/${_OLD_PROJECT}.colors"
        /usr/lib/cmake/"${OLD_PROJECT^}"
        "/usr/lib/x86_64-linux-gnu/qt6/plugins/org.kde.kdecoration3/org.kde.${OLD_PROJECT}.so*"
        "/usr/lib/x86_64-linux-gnu/qt6/plugins/kstyle_config/${OLD_PROJECT}styleconfig.so*"
        "/usr/lib/x86_64-linux-gnu/qt6/plugins/org.kde.kdecoration3.kcm/kcm_${OLD_PROJECT}decoration.so*"
        "/usr/lib/x86_64-linux-gnu/qt6/plugins/styles/${OLD_PROJECT}6.so*"
        "/usr/share/kservices6/${PROJECT}decorationconfig.desktop"
    )

    # Intentional glob expansion for the fixed system-path patterns above.
    for f in ${files[@]}; do
        sudo rm -rf "$f"
    done
}

# if existing
remove_qt5_files() {
    local f
    local files=(
        "/usr/lib64/qt5/plugins/styles/${PROJECT}5.so*"
        "/usr/lib/qt5/plugins/styles/${PROJECT}5.so*"
        "/usr/lib64/lib${PROJECT}common5.so*"
        "/usr/lib/lib${PROJECT}common5.so*"
        "/usr/lib64/lib${PROJECT}common5.so*"
        "/usr/lib/lib${PROJECT}common5.so*"
        "/usr/lib64/qt/plugins/styles/${PROJECT}5.so*"
        "/usr/lib/x86_64-linux-gnu/qt5/plugins/styles/${PROJECT}5.so*"
        "/usr/lib64/qt5/plugins/styles/${OLD_PROJECT}5.so*"
        "/usr/lib/qt5/plugins/styles/${OLD_PROJECT}5.so*"
        "/usr/lib64/lib${OLD_PROJECT}common5.so*"
        "/usr/lib/lib${OLD_PROJECT}common5.so*"
        "/usr/lib64/lib${OLD_PROJECT}common5.so*"
        "/usr/lib/lib${OLD_PROJECT}common5.so*"
        "/usr/lib64/qt/plugins/styles/${OLD_PROJECT}5.so*"
        "/usr/lib/x86_64-linux-gnu/qt5/plugins/styles/${OLD_PROJECT}5.so*"
    )

    # Intentional glob expansion for the fixed system-path patterns above.
    for f in ${files[@]}; do
        sudo rm -rf "$f"
    done
}

case "$QT_BUILD" in
    qt5|QT5) build_glass -DBUILD_QT6=OFF -DBUILD_QT5=ON ;;
    qt6|QT6) build_glass -DBUILD_QT6=ON -DBUILD_QT5=OFF ;;
    default) build_glass ;;
    remove)
        remove_qt5_files
        remove_qt6_files
        sudo rm -f -- /usr/share/color-schemes/GlassDarkFixed.colors
        if (( EUID == 0 )); then
            printf 'System Glass files removed. To remove the per-user accent helper, run as the desktop user (without sudo):\n'
            printf '  %q remove\n' "$SRC_DIR/tools/glass-user-helper"
        else
            "$SRC_DIR/tools/glass-user-helper" remove
        fi
        printf 'Glass removal completed.\n'
        ;;
    helper|user-helper) "$SRC_DIR/tools/glass-user-helper" install ;;
    *) printf 'Usage: %s [QT5|QT6|helper|remove]\n' "$0" >&2; exit 2 ;;
esac
