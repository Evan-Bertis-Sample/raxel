#!/usr/bin/env bash
#
# install_raxel.sh
#
# This script installs a "raxel" command so you can run raxel.py from anywhere.

# 1) The path to your raxel.py. Adjust to wherever it resides:
RAXEL_PY_PATH="$(pwd)/raxel.py"

# 2) Decide where you want to install the symlink or wrapper script.
#    Typically on macOS/Linux, /usr/local/bin is in $PATH.
INSTALL_DIR="/usr/local/bin"

# 3) Detect OS
OS="$(uname -s 2>/dev/null || echo Windows)"

echo "Detected OS: ${OS}"

# Check that raxel.py exists
if [ ! -f "${RAXEL_PY_PATH}" ]; then
  echo "ERROR: Cannot find raxel.py at: ${RAXEL_PY_PATH}"
  exit 1
fi

if [[ "${OS}" == "Darwin" || "${OS}" == "Linux" ]]; then
  echo "Installing on macOS/Linux..."

  # Ensure raxel.py is executable
  chmod +x "${RAXEL_PY_PATH}"

  # If /usr/local/bin/raxel already exists, remove it (or back it up).
  if [ -L "${INSTALL_DIR}/raxel" ] || [ -f "${INSTALL_DIR}/raxel" ]; then
    echo "Removing existing ${INSTALL_DIR}/raxel"
    sudo rm -f "${INSTALL_DIR}/raxel"
  fi

  # Create the symlink
  echo "Creating symlink: ${INSTALL_DIR}/raxel -> ${RAXEL_PY_PATH}"
  sudo ln -s "${RAXEL_PY_PATH}" "${INSTALL_DIR}/raxel"

  echo "Installation complete. You should now be able to run 'raxel' anywhere."

elif [[ "${OS}" =~ ^CYGWIN || "${OS}" =~ ^MINGW || "${OS}" == "Windows" ]]; then
  echo "Installing on Windows..."

  # On Windows, creating a symlink in the standard system PATH is less common
  # (and can require admin privileges). Instead, we can generate a .cmd wrapper
  # if the user has a directory in PATH for local scripts. E.g. %USERPROFILE%\bin

  # 1) Where can we place the wrapper? We’ll pick something like %USERPROFILE%\bin.
  #    If that folder doesn't exist or isn't in PATH, user must manually add it.
  USER_BIN_WIN="$HOME/bin"   # For Git Bash, Cygwin, or MINGW, $HOME points to user dir

  if [ ! -d "$USER_BIN_WIN" ]; then
    echo "Creating directory: $USER_BIN_WIN"
    mkdir -p "$USER_BIN_WIN"
  fi

  cat <<EOF > "$USER_BIN_WIN/raxel.cmd"
@echo off
REM This wrapper script calls Python on raxel.py
python "$(cygpath -w "${RAXEL_PY_PATH}")" %*
EOF

  echo "Created $USER_BIN_WIN/raxel.cmd"
  echo ""
  echo "Next steps (Windows user):"
  echo "  1) Ensure $USER_BIN_WIN is in your PATH."
  echo "  2) Then you can run 'raxel' from any command prompt."
  echo ""
  echo "Installation complete."


  # for bash users, we can also create a bash script
  USER_BIN_BASH="$HOME/bin"
  if [ ! -d "$USER_BIN_BASH" ]; then
    echo "Creating directory: $USER_BIN_BASH"
    mkdir -p "$USER_BIN_BASH"
  fi

  cat <<EOF > "$USER_BIN_BASH/raxel"
#!/usr/bin/env bash
# This wrapper script calls Python on raxel.py
python "${RAXEL_PY_PATH}" "\$@"
EOF

  chmod +x "$USER_BIN_BASH/raxel"

  echo "Created $USER_BIN_BASH/raxel"
  echo ""
  echo "Next steps (bash user):"
  echo "  1) Ensure $USER_BIN_BASH is in your PATH."
  echo "  2) Then you can run 'raxel' from any command prompt."
  echo ""
  echo "Installation complete."


else
  echo "ERROR: Unrecognized OS: ${OS}"
  echo "Please edit this script to handle your environment."
  exit 1
fi
