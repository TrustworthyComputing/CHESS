#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"

usage() {
  cat <<'EOF'
Usage: run.sh <output_cpp_name_or_path>

If a bare name is provided, the input file is resolved as:
  examples/output/<name>.cpp
EOF
}

if [[ $# -lt 1 ]]; then
  usage
  exit 1
fi

input_arg="$1"
shift
if [[ "${input_arg}" == */* ]]; then
  output_cpp="${input_arg}"
elif [[ "${input_arg}" == *.cpp ]]; then
  output_cpp="${repo_root}/examples/output/${input_arg}"
else
  output_cpp="${repo_root}/examples/output/${input_arg}.cpp"
fi

if [[ ! -f "${output_cpp}" ]]; then
  echo "Output C++ not found: ${output_cpp}" >&2
  exit 1
fi

cxx="${CXX:-clang++}"
cxxflags="${CXXFLAGS:--O2 -std=c++17}"
openfhe_include="${OPENFHE_INCLUDE_DIR:-/usr/local/include/openfhe}"
openfhe_lib_dir="${OPENFHE_LIB_DIR:-/usr/local/lib}"
openfhe_libs="${OPENFHE_LIBS:- -lOPENFHEpke -lOPENFHEbinfhe -lOPENFHEcore -ldl}"

include_flags=(
  "-I${openfhe_include}"
  "-I${openfhe_include}/third-party/include"
  "-I${openfhe_include}/core"
  "-I${openfhe_include}/pke"
  "-I${openfhe_include}/binfhe"
)

output_bin="$(mktemp -t scheme_switching_run.XXXXXX)"
src_backend=(
  "${repo_root}/src/backend/ckks_operations.cpp"
  "${repo_root}/src/backend/cggi_operations.cpp"
)

"${cxx}" ${cxxflags} \
  "${output_cpp}" \
  "${src_backend[@]}" \
  "${include_flags[@]}" \
  -L"${openfhe_lib_dir}" -Wl,-rpath,"${openfhe_lib_dir}" \
  ${openfhe_libs} \
  -o "${output_bin}"

"${output_bin}" "$@"
