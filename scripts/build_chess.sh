#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

usage() {
  cat <<'EOF'
Usage: build_chess.sh <gcc|clang> [build_dir] [extra_cmake_args...]

Examples:
  ./scripts/build_chess.sh clang
  ./scripts/build_chess.sh gcc /tmp/chess-build -DOpenFHE_DIR=/opt/openfhe/lib/cmake/OpenFHE
EOF
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

compiler="$1"
shift

case "${compiler}" in
  gcc)
    c_compiler="gcc"
    cxx_compiler="g++"
    ;;
  clang)
    c_compiler="clang"
    cxx_compiler="clang++"
    ;;
  *)
    echo "Unknown compiler: ${compiler}" >&2
    usage
    exit 1
    ;;
esac

build_dir="${1:-${repo_root}/build/chess}"
if [[ $# -gt 0 ]]; then
  shift
fi

cmake -S "${repo_root}" -B "${build_dir}" \
  -DBUILD_MLIR_COMPILER=ON \
  -DCMAKE_C_COMPILER="${c_compiler}" \
  -DCMAKE_CXX_COMPILER="${cxx_compiler}" \
  "$@"
cmake --build "${build_dir}" --target chess
