#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

usage() {
  cat <<'EOF'
Usage: build_chess.sh <gcc|clang> [build_dir] [--no-backend] [extra_cmake_args...]

Examples:
  ./scripts/build_chess.sh clang
  ./scripts/build_chess.sh gcc /tmp/chess-build --no-backend -DOpenFHE_DIR=/opt/openfhe/lib/cmake/OpenFHE
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

build_dir=""
no_backend=0
extra_args=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-backend)
      no_backend=1
      shift
      ;;
    --)
      shift
      extra_args+=("$@")
      break
      ;;
    -*)
      extra_args+=("$1")
      shift
      ;;
    *)
      if [[ -z "${build_dir}" ]]; then
        build_dir="$1"
        shift
      else
        extra_args+=("$1")
        shift
      fi
      ;;
  esac
done

build_dir="${build_dir:-${repo_root}/build/chess}"

cmake -S "${repo_root}" -B "${build_dir}" \
  -DBUILD_MLIR_COMPILER=ON \
  -DCMAKE_C_COMPILER="${c_compiler}" \
  -DCMAKE_CXX_COMPILER="${cxx_compiler}" \
  "${extra_args[@]}"
if [[ "${no_backend}" -eq 0 ]]; then
  cmake --build "${build_dir}" --target scheme_switching
fi
cmake --build "${build_dir}" --target chess
