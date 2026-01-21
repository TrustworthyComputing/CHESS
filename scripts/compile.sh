#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
mlir_dir="${repo_root}/examples/mlir"
output_dir="${repo_root}/examples/output"

usage() {
  cat <<'EOF'
Usage: compile.sh <mlir_filename>

The input file is resolved as: examples/mlir/<mlir_filename>
Output C++ is written to:     examples/output/<basename>.cpp

Environment overrides:
  CHESS_BIN            Path to the chess binary (default: build/chess or build/compiler/chess)
  MLIR_TRANSLATE_BIN   Path to mlir-translate (default: /home/rostin/Polygeist/build/bin/mlir-translate)
EOF
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

input_name="$1"
input_path="${mlir_dir}/${input_name}"
if [[ ! -f "${input_path}" ]]; then
  echo "MLIR input not found: ${input_path}" >&2
  exit 1
fi

mkdir -p "${output_dir}"

chess_bin="${CHESS_BIN:-}"
if [[ -z "${chess_bin}" ]]; then
  if [[ -x "${repo_root}/build/chess" ]]; then
    chess_bin="${repo_root}/build/chess"
  elif [[ -x "${repo_root}/build/compiler/chess" ]]; then
    chess_bin="${repo_root}/build/compiler/chess"
  else
    echo "chess binary not found. Build it with ./scripts/build_chess.sh <gcc|clang>" >&2
    exit 1
  fi
fi

base_name="$(basename "${input_path}")"
base_name="${base_name%.mlir}"
transformed_mlir="${mlir_dir}/${base_name}.ir.mlir"
output_cpp="${output_dir}/${base_name}.cpp"

"${chess_bin}" "${input_path}" "${transformed_mlir}"

mlir_translate_bin="${MLIR_TRANSLATE_BIN:-/home/rostin/Polygeist/build/bin/mlir-translate}"
if [[ ! -x "${mlir_translate_bin}" ]]; then
  if command -v mlir-translate >/dev/null 2>&1; then
    mlir_translate_bin="$(command -v mlir-translate)"
  else
    echo "mlir-translate not found. Set MLIR_TRANSLATE_BIN or add it to PATH." >&2
    exit 1
  fi
fi

"${mlir_translate_bin}" -allow-unregistered-dialect --mlir-to-cpp "${transformed_mlir}" -o "${output_cpp}"

echo "Wrote ${output_cpp}"
