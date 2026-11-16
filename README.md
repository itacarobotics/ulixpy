# ulixarm_py

Python bindings (`ulixpy`) for the UlixArm C++ library.

Supported platform: **Ubuntu (or another apt-based distribution) on amd64**.

## Install with curl (recommended)

Run as a normal user (not as root and not with `sudo`):

```bash
curl -fsSL https://itacarobotics.com/install.sh | bash
```

The installer asks for your password once, for the steps that need administrator
access, and finishes in a shell with the `ulixenv` environment already active.

### What the installer does

The script is [`install.sh`](install.sh). It runs these steps in order:

| Step | What it installs | Where |
| --- | --- | --- |
| 1. Pinocchio | Adds the robotpkg apt repository (key in `/etc/apt/keyrings/robotpkg.asc`, source in `/etc/apt/sources.list.d/robotpkg.list`) and installs `robotpkg-py3*-pinocchio` | System-wide, under `/opt/openrobots` |
| 2. Build dependencies | `build-essential`, `cmake`, `git`, `pkg-config`, `libeigen3-dev`, `libserialport-dev` via apt | System-wide (`/usr`) |
| 3. Miniforge | A conda distribution, only if `conda` is not already installed. Runs `conda init` for your shell, which adds a block to `~/.bashrc` | `~/miniforge3` |
| 4. Conda environment | Creates the `ulixenv` environment with Python 3.10 and sets `CMAKE_PREFIX_PATH`, `PKG_CONFIG_PATH` and `LD_LIBRARY_PATH` to Pinocchio's location. These variables are only set while `ulixenv` is active | `~/miniforge3/envs/ulixenv` (or your existing conda's `envs/`) |
| 5. Source code | Clones this repository with its `ulixarm_cpp` submodule (or pulls the latest version if already cloned). The directory is owned by your user | `/opt/itacarobotics/ulixpy` |
| 6. Build | Runs `pip install .` inside `ulixenv`, which compiles the C++ library and installs `ulixpy` together with `numpy` and `pin` (Pinocchio's Python wheel) | `ulixenv`'s `site-packages` |
| 7. Check | Runs `import ulixpy; ulixpy.UlixArm()` to confirm the installation works | – |

Output from each step goes to a log in `/tmp/ulixpy-install.*.log`. The log is
deleted on success and its path is printed if a step fails.

Running the installer again is safe: existing packages, the conda environment
and the cloned repository are reused, and the repository is updated to the
latest version before rebuilding.

### Using the environment

In every new terminal, activate the environment before using ulixpy:

```bash
conda activate ulixenv
```

To leave it, run `conda deactivate`.

If `conda` is not found, run `source ~/.bashrc` or open a new terminal.

### Uninstall

```bash
conda env remove -n ulixenv            # the Python environment and ulixpy
rm -rf /opt/itacarobotics/ulixpy       # the source code
rm -rf ~/miniforge3                    # only if the installer installed Miniforge
```

The apt packages from steps 1 and 2 can be removed with `sudo apt remove`.

## Install with pip

`ulixpy` is compiled from source when installed with pip, so the system
dependencies have to be installed first. Use this if you want ulixpy in your own
Python environment (venv, conda, ...) instead of `ulixenv`. Python 3.10 or newer
is required.

### 1. System dependencies

Pinocchio, from the robotpkg apt repository:

```bash
sudo apt install -y lsb-release curl
sudo mkdir -p /etc/apt/keyrings
curl -fsSL http://robotpkg.openrobots.org/packages/debian/robotpkg.asc \
    | sudo tee /etc/apt/keyrings/robotpkg.asc
echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/robotpkg.asc] http://robotpkg.openrobots.org/packages/debian/pub $(lsb_release -cs) robotpkg" \
    | sudo tee /etc/apt/sources.list.d/robotpkg.list
sudo apt update
sudo apt install -y 'robotpkg-py3*-pinocchio'
```

Compiler, CMake, Eigen and libserialport:

```bash
sudo apt install -y build-essential cmake git pkg-config libeigen3-dev libserialport-dev
```

### 2. Point the build at Pinocchio

robotpkg installs into `/opt/openrobots`, which CMake does not search by default.
Set these variables in the shell where you run pip (and keep `LD_LIBRARY_PATH`
set wherever you use ulixpy):

```bash
export CMAKE_PREFIX_PATH=/opt/openrobots
export PKG_CONFIG_PATH=/opt/openrobots/lib/pkgconfig
export LD_LIBRARY_PATH=/opt/openrobots/lib
```

Do not add `/opt/openrobots` to `PYTHONPATH`: ulixpy uses the `pin` wheel from
PyPI for Pinocchio's Python bindings.

### 3. Install

Activate your Python environment, then either install directly from GitHub:

```bash
pip install "git+https://github.com/itacarobotics/ulixpy.git"
```

or from a local clone (the `--recursive` flag is required to fetch the
`ulixarm_cpp` submodule):

```bash
git clone --recursive https://github.com/itacarobotics/ulixpy.git
cd ulixpy
pip install .
```

pip downloads the build tools (`scikit-build-core`, `pybind11`, `pin`) into a
temporary build environment, compiles the C++ code, and installs `ulixpy`,
`numpy` and `pin==4.1.0` into the active environment. The build takes a few
minutes.

Check the installation (run it outside the source directory):

```bash
python -c "import ulixpy; ulixpy.UlixArm()"
```
