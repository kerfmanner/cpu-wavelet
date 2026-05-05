#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_root="$(cd -- "${script_dir}/.." && pwd)"

build_type="Release"
build_dir=""
target="cpu_wavelet_cli"
generator="Ninja"
use_openmp="OFF"
clean="OFF"
configure_only="OFF"
parallel_jobs=""
extra_cmake_args=()

usage() {
  cat <<'EOF'
Usage: cpu-wavelet/scripts/build.sh [options]

Options:
  --debug                  Build Debug.
  --release                Build Release (default).
  --relwithdebinfo         Build RelWithDebInfo.
  --minsizerel             Build MinSizeRel.
  --openmp                 Enable OpenMP executor support.
  --no-openmp              Disable OpenMP executor support (default).
  --target NAME            CMake target to build (default: cpu_wavelet_cli).
  --all                    Build all default targets.
  --build-dir DIR          Override build directory.
  --generator NAME         CMake generator (default: Ninja).
  --clean                  Remove build directory before configuring.
  --configure-only         Configure but do not build.
  -j, --jobs N             Parallel build jobs.
  -DVAR=VALUE              Extra CMake cache definition.
  -h, --help               Show this help message.

Examples:
  cpu-wavelet/scripts/build.sh --release
  cpu-wavelet/scripts/build.sh --debug --target cpu_wavelet_cli
  cpu-wavelet/scripts/build.sh --release --openmp -j 8
  cpu-wavelet/scripts/build.sh --relwithdebinfo -DCMAKE_CXX_COMPILER=clang++
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug)
      build_type="Debug"
      ;;
    --release)
      build_type="Release"
      ;;
    --relwithdebinfo)
      build_type="RelWithDebInfo"
      ;;
    --minsizerel)
      build_type="MinSizeRel"
      ;;
    --openmp)
      use_openmp="ON"
      ;;
    --no-openmp)
      use_openmp="OFF"
      ;;
    --target)
      shift
      [[ $# -gt 0 ]] || { echo "error: --target expects a value" >&2; exit 2; }
      target="$1"
      ;;
    --all)
      target=""
      ;;
    --build-dir)
      shift
      [[ $# -gt 0 ]] || { echo "error: --build-dir expects a value" >&2; exit 2; }
      build_dir="$1"
      ;;
    --generator)
      shift
      [[ $# -gt 0 ]] || { echo "error: --generator expects a value" >&2; exit 2; }
      generator="$1"
      ;;
    --clean)
      clean="ON"
      ;;
    --configure-only)
      configure_only="ON"
      ;;
    -j|--jobs)
      shift
      [[ $# -gt 0 ]] || { echo "error: --jobs expects a value" >&2; exit 2; }
      parallel_jobs="$1"
      ;;
    -D*)
      extra_cmake_args+=("$1")
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

if [[ -z "${build_dir}" ]]; then
  build_dir="${project_root}/build/${build_type}"
  if [[ "${use_openmp}" == "ON" ]]; then
    build_dir="${build_dir}-openmp"
  fi
fi

if [[ "${clean}" == "ON" ]]; then
  rm -rf -- "${build_dir}"
fi

cmake_args=(
  -S "${project_root}"
  -B "${build_dir}"
  -G "${generator}"
  -DCMAKE_BUILD_TYPE="${build_type}"
  -DCPU_WAVELET_USE_OPENMP="${use_openmp}"
)

cmake_args+=("${extra_cmake_args[@]}")

echo "==> Configuring cpu-wavelet"
echo "    build_type=${build_type}"
echo "    build_dir=${build_dir}"
echo "    generator=${generator}"
echo "    openmp=${use_openmp}"
cmake "${cmake_args[@]}"

if [[ "${configure_only}" == "ON" ]]; then
  exit 0
fi

build_args=(--build "${build_dir}")
if [[ -n "${target}" ]]; then
  build_args+=(--target "${target}")
fi
if [[ -n "${parallel_jobs}" ]]; then
  build_args+=(--parallel "${parallel_jobs}")
fi

echo "==> Building cpu-wavelet"
cmake "${build_args[@]}"

cat <<EOF
==> Done
Binary location is generator-dependent. Common paths:
  ${build_dir}/cpu_wavelet_cli
  ${build_dir}/apps/cpu_wavelet_cli
EOF
