#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
build_dir="${repo_root}/build/run"

usage() {
  cat <<'EOF'
Usage: run.sh <output_cpp_name>

The input file is resolved as: examples/output/<output_cpp_name>
You can pass either "min_index" or "min_index.cpp".
EOF
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

output_name="$1"
if [[ "${output_name}" != *.cpp ]]; then
  output_name="${output_name}.cpp"
fi

output_cpp="${repo_root}/examples/output/${output_name}"
if [[ ! -f "${output_cpp}" ]]; then
  echo "Output C++ not found: ${output_cpp}" >&2
  exit 1
fi

cmake -S "${repo_root}" -B "${build_dir}" \
  -DNATIVE_SIZE=32 \
  -DCMAKE_C_COMPILER=clang-12 \
  -DCMAKE_CXX_COMPILER=clang++-12 \
  -DWITH_NTL=ON \
  -DOMP_NUM_THREADS=24 \
  -DRUN_OUTPUT_CPP="${output_cpp}"

cmake --build "${build_dir}" --target run_output
"${build_dir}/run_output"
