#!/usr/bin/env bash
#
# ulixpy installer
#
#   curl -fsSL https://raw.githubusercontent.com/itacarobotics/ulixpy/main/install.sh | bash
#
# Run as a normal user: sudo is used only for the steps that need it.
# Everything happens inside main(), called on the last line, so a truncated
# download cannot run half a script and commands cannot consume it from stdin.

set -euo pipefail

REPO_URL="https://github.com/itacarobotics/ulixpy.git"
INSTALL_ROOT="/opt/itacarobotics"
REPO_DIR="${INSTALL_ROOT}/ulixpy"
ENV_NAME="ulixenv"
PYTHON_VERSION="3.10"
MINIFORGE_DIR="${HOME}/miniforge3"
MINIFORGE_URL="https://github.com/conda-forge/miniforge/releases/latest/download"

CONDA=""
CONDA_INSTALLED=0
LOG=""
WORK_DIR=""
SUDO_KEEPALIVE_PID=""
STEP_PID=""

# ---------------------------------------------------------------- output ----

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    BOLD=$'\033[1m' DIM=$'\033[2m' RED=$'\033[31m' GREEN=$'\033[32m' RESET=$'\033[0m'
else
    BOLD="" DIM="" RED="" GREEN="" RESET=""
fi

die() {
    printf '\n%sError:%s %s\n' "${RED}${BOLD}" "${RESET}" "$1" >&2
    shift
    for line in "$@"; do
        printf '%s\n' "${line}" >&2
    done
    exit 1
}

show_cursor() {
    if [ -t 1 ]; then printf '\033[?25h'; fi
}

cleanup() {
    local status=$?
    trap - EXIT
    if [ -n "${STEP_PID}" ]; then kill "${STEP_PID}" 2>/dev/null || true; fi
    if [ -n "${SUDO_KEEPALIVE_PID}" ]; then kill "${SUDO_KEEPALIVE_PID}" 2>/dev/null || true; fi
    if [ -n "${WORK_DIR}" ]; then rm -rf "${WORK_DIR}"; fi
    if [ "${status}" -eq 0 ] && [ -n "${LOG}" ]; then rm -f "${LOG}"; fi
    show_cursor
    exit "${status}"
}

# step "label" command...: runs quietly with a spinner; output goes to the log.
step() {
    local label=$1 i=0
    local frames=('⠋' '⠙' '⠹' '⠸' '⠼' '⠴' '⠦' '⠧' '⠇' '⠏')
    shift

    printf '\n==> %s\n' "${label}" >>"${LOG}"
    "$@" >>"${LOG}" 2>&1 </dev/null &
    STEP_PID=$!

    if [ -t 1 ]; then
        printf '\033[?25l'
        while kill -0 "${STEP_PID}" 2>/dev/null; do
            printf '\r  %s %s' "${frames[i++ % ${#frames[@]}]}" "${label}"
            sleep 0.1
        done
        printf '\r\033[K'
        show_cursor
    fi

    local status=0
    wait "${STEP_PID}" || status=$?
    STEP_PID=""

    if [ "${status}" -ne 0 ]; then
        printf '  %s✗%s %s\n\n' "${RED}" "${RESET}" "${label}"
        tail -n 20 "${LOG}" | sed 's/^/    /' >&2
        die "'${label}' failed." "Full log: ${LOG}"
    fi
    printf '  %s✓%s %s\n' "${GREEN}" "${RESET}" "${label}"
}

# ---------------------------------------------------------------- checks ----

find_conda() {
    if [ -n "${CONDA_EXE:-}" ] && [ -x "${CONDA_EXE}" ]; then
        echo "${CONDA_EXE}"
    elif [ -x "${MINIFORGE_DIR}/bin/conda" ]; then
        echo "${MINIFORGE_DIR}/bin/conda"
    elif command -v conda >/dev/null 2>&1; then
        command -v conda
    fi
}

preflight() {
    [ "$(id -u)" -ne 0 ] \
        || die "Run this installer as a normal user, not as root or with sudo."
    command -v apt-get >/dev/null 2>&1 \
        || die "This installer needs an apt-based distribution (Ubuntu)."
    command -v curl >/dev/null 2>&1 \
        || die "curl is required." "Install it with: sudo apt install curl"
    [ "$(dpkg --print-architecture)" = "amd64" ] \
        || die "Pinocchio's apt packages are only published for amd64."
}

# Asks for the password once, then keeps sudo alive through the long build.
acquire_sudo() {
    if ! sudo -n true 2>/dev/null; then
        echo "  Administrator access is needed to install system packages."
        sudo -v || die "Could not get administrator access."
    fi
    while sudo -n true 2>/dev/null; do
        sleep 60
    done </dev/null >/dev/null 2>&1 &
    SUDO_KEEPALIVE_PID=$!
}

# ----------------------------------------------------------------- steps ----

install_pinocchio() {
    sudo apt-get install -y lsb-release curl
    sudo mkdir -p /etc/apt/keyrings
    curl -fsSL http://robotpkg.openrobots.org/packages/debian/robotpkg.asc \
        | sudo tee /etc/apt/keyrings/robotpkg.asc >/dev/null
    echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/robotpkg.asc] http://robotpkg.openrobots.org/packages/debian/pub $(lsb_release -cs) robotpkg" \
        | sudo tee /etc/apt/sources.list.d/robotpkg.list >/dev/null
    sudo apt-get update
    sudo apt-get install -y 'robotpkg-py3*-pinocchio'
}

install_system_packages() {
    sudo apt-get install -y build-essential cmake git pkg-config \
        libeigen3-dev libserialport-dev
}

install_miniforge() {
    local installer="${WORK_DIR}/miniforge.sh"
    curl -fsSL -o "${installer}" "${MINIFORGE_URL}/Miniforge3-Linux-$(uname -m).sh"
    bash "${installer}" -b -p "${MINIFORGE_DIR}"
    "${MINIFORGE_DIR}/bin/conda" init "$(basename "${SHELL:-bash}")"
}

create_env() {
    if ! "${CONDA}" env list | awk '{print $1}' | grep -qx "${ENV_NAME}"; then
        "${CONDA}" create -y -n "${ENV_NAME}" "python=${PYTHON_VERSION}"
    fi

    # Scoped to the environment so the rest of the system is left alone.
    # PYTHONPATH is left out on purpose: robotpkg's Python bindings target the
    # system Python, while ulixpy gets its bindings from the pin wheel.
    local prefix
    prefix="$(dpkg -L robotpkg-pinocchio \
        | sed -n -E 's#/lib/cmake/pinocchio/pinocchio(Config|-config)\.cmake$##p' \
        | head -n 1)"
    [ -n "${prefix}" ] || { echo "Could not locate Pinocchio's CMake config."; return 1; }

    "${CONDA}" env config vars set -n "${ENV_NAME}" \
        "CMAKE_PREFIX_PATH=${prefix}" \
        "PKG_CONFIG_PATH=${prefix}/lib/pkgconfig" \
        "LD_LIBRARY_PATH=${prefix}/lib"
}

fetch_repo() {
    sudo mkdir -p "${INSTALL_ROOT}"
    sudo chown "$(id -un):$(id -gn)" "${INSTALL_ROOT}"
    if [ -d "${REPO_DIR}/.git" ]; then
        git -C "${REPO_DIR}" pull --ff-only
    else
        git clone "${REPO_URL}" "${REPO_DIR}"
    fi
    git -C "${REPO_DIR}" submodule update --init --recursive
}

install_package() {
    cd "${REPO_DIR}"
    "${CONDA}" run --no-capture-output -n "${ENV_NAME}" python -m pip install .
}

verify_install() {
    cd "${WORK_DIR}"
    "${CONDA}" run --no-capture-output -n "${ENV_NAME}" python -c "import ulixpy"
}

# ------------------------------------------------------------------ main ----

main() {
    if [ "$(uname -s)" != "Linux" ]; then
        echo "COMING SOON FOR YOU :)"
        exit 0
    fi

    trap cleanup EXIT
    trap 'exit 130' INT TERM

    printf '\n%sInstalling ulixpy%s\n\n' "${BOLD}" "${RESET}"

    preflight
    LOG="$(mktemp "${TMPDIR:-/tmp}/ulixpy-install.XXXXXX.log")"
    WORK_DIR="$(mktemp -d)"
    acquire_sudo

    step "Installing Pinocchio" install_pinocchio
    step "Installing Eigen, libserialport and build tools" install_system_packages

    CONDA="$(find_conda)"
    if [ -z "${CONDA}" ]; then
        step "Installing Miniforge" install_miniforge
        CONDA="${MINIFORGE_DIR}/bin/conda"
        CONDA_INSTALLED=1
    fi

    step "Setting up conda environment '${ENV_NAME}'" create_env
    step "Downloading ulixpy" fetch_repo
    step "Building ulixpy (this can take a few minutes)" install_package
    step "Checking the installation" verify_install

    printf '\n%sulixpy is installed.%s\n\n' "${GREEN}${BOLD}" "${RESET}"
    if [ "${CONDA_INSTALLED}" -eq 1 ]; then
        printf '  Open a new terminal, then run:\n\n'
    else
        printf '  Get started with:\n\n'
    fi
    printf '    %sconda activate %s%s\n\n' "${BOLD}" "${ENV_NAME}" "${RESET}"
    printf '  %sSource: %s%s\n\n' "${DIM}" "${REPO_DIR}" "${RESET}"
}

main "$@"
