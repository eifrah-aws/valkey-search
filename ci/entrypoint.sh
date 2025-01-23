#!/bin/bash
set -e
export MOUNTED_DIR=$(pwd)
echo "export MOUNTED_DIR=$MOUNTED_DIR" >> /home/vscode/.bashrc
if [ "$ENABLE_COMP_DB_REFRESH" = "true" ]; then
    echo "$(pwd)/ci/refresh_comp_db.sh &> /dev/null &" >> /home/vscode/.bashrc
fi
bazel build //...
bazel run @hedron_compile_commands//:refresh_all