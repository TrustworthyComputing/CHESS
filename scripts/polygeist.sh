#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

usage() {
  cat <<'EOF'
Usage: polygeist.sh <mlir_name> [input_cpp]

mlir_name can be a basename (e.g., min_index) or a path ending in .mlir.
The final MLIR is written to examples/mlir/.
input_cpp defaults to test/test.cpp.
EOF
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

mlir_name="$1"
input_cpp="${2:-${repo_root}/test/test.cpp}"

if [[ "${mlir_name}" == *.mlir ]]; then
  base_name="$(basename "${mlir_name}")"
else
  base_name="${mlir_name}.mlir"
fi

output_dir="${repo_root}/examples/mlir"
mlir_path="${output_dir}/${base_name}"

if [[ ! -f "${input_cpp}" ]]; then
  echo "Input C++ not found: ${input_cpp}" >&2
  exit 1
fi

mkdir -p "${output_dir}"
temp_mlir="$(mktemp -t polygeist.XXXXXX.mlir)"
temp_vec_mlir="$(mktemp -t polygeist.XXXXXX.mlir)"
trap 'rm -f "${temp_mlir}" "${temp_vec_mlir}"' EXIT

# clang++ -S -emit-llvm -O3 "${input_cpp}" -o "${mlir_path%.mlir}.ll"
cgeist "${input_cpp}" -S -O3 -raise-scf-to-affine --polyhedral-opt > "${temp_mlir}"
mlir-opt -affine-super-vectorize="virtual-vector-size=10 test-fastest-varying=0 vectorize-reductions=true" \
  "${temp_mlir}" -o "${temp_vec_mlir}"
mlir-opt -lower-affine "${temp_vec_mlir}" -o "${mlir_path}"
