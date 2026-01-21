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
  MLIR_OPT_BIN         Path to mlir-opt
  POLYGEIST_ROOT       Prefix that contains bin/mlir-opt
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

mlir_opt_bin="${MLIR_OPT_BIN:-}"
if [[ -z "${mlir_opt_bin}" && -n "${POLYGEIST_ROOT:-}" ]]; then
  mlir_opt_bin="${POLYGEIST_ROOT}/bin/mlir-opt"
fi
if [[ -z "${mlir_opt_bin}" ]]; then
  if command -v mlir-opt >/dev/null 2>&1; then
    mlir_opt_bin="$(command -v mlir-opt)"
  else
    echo "mlir-opt not found. Set MLIR_OPT_BIN, POLYGEIST_ROOT, or add it to PATH." >&2
    exit 1
  fi
fi

chess_bin="${CHESS_BIN:-}"
if [[ -z "${chess_bin}" ]]; then
  if [[ -x "${repo_root}/build/chess/chess" ]]; then
    chess_bin="${repo_root}/build/chess/chess"
  elif [[ -x "${repo_root}/build/chess" ]]; then
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
pre_lowered_mlir="${mlir_dir}/${base_name}.lowered_in.mlir"
output_cpp="${output_dir}/${base_name}.cpp"

"${mlir_opt_bin}" -lower-affine "${input_path}" -o "${pre_lowered_mlir}"
"${chess_bin}" "${pre_lowered_mlir}" "${output_cpp}"

echo "Wrote ${output_cpp}"
