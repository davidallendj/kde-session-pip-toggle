#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="$project_dir/build"

cmake -S "$project_dir" -B "$build_dir"
if [ -n "${CMAKE_BUILD_PARALLEL_LEVEL:-}" ]; then
    cmake --build "$build_dir" --parallel
else
    cmake --build "$build_dir"
fi

printf '%s\n' "Built: $build_dir/kde-pip-session"
