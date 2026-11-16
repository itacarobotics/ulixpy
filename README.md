# ulixarm_py

Python bindings (`ulixpy`) for the UlixArm C++ library.

Supported platform: **Ubuntu (or another apt-based distribution) on x86_64 or aarch64**.

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
| 1. Build dependencies | `build-essential`, `cmake`, `git`, `pkg-config`, `libeigen3-dev`, `libserialport-dev` via apt | System-wide (`/usr`) |
| 2. Miniforge | A conda distribution, only if `conda` is not already installed. Runs `conda init` for your shell, which adds a block to `~/.bashrc` | `~/miniforge3` |
| 3. Conda environment | Creates the `ulixenv` environment with Python 3.10 | `~/miniforge3/envs/ulixenv` (or your existing conda's `envs/`) |
| 4. Source code | Clones this repository with its `ulixarm_cpp` submodule (or pulls the latest version if already cloned). The directory is owned by your user | `/opt/itacarobotics/ulixpy` |
| 5. Build | Runs `pip install .` inside `ulixenv`, which compiles the C++ library and installs `ulixpy` together with `numpy` and `pin`. The `pin` wheel provides Pinocchio, both its Python module and the C++ library ulixpy links against | `ulixenv`'s `site-packages` (Pinocchio's C++ files are in `site-packages/cmeel.prefix`) |
| 6. Check | Runs `import ulixpy; ulixpy.UlixArm()` to confirm the installation works | – |

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

The apt packages from step 1 can be removed with `sudo apt remove`.

## Install with pip

`ulixpy` is compiled from source when installed with pip. Use this if you want
ulixpy in your own Python environment (venv, conda, ...) instead of `ulixenv`.
Python 3.10 or newer is required.

Pinocchio does not need to be installed separately: it comes from the `pin`
wheel on PyPI, which pip installs automatically and which contains both
Pinocchio's Python module and its C++ library.

### 1. System dependencies

Compiler, CMake, Eigen and libserialport:

```bash
sudo apt install -y build-essential cmake git pkg-config libeigen3-dev libserialport-dev
```

### 2. Install

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
