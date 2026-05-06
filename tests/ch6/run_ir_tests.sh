#!/bin/sh
set -eu

compiler=${1:-bin/a.out}
outdir=${2:-bin/testoutput/ch6}
mkdir -p "$outdir"

failures=0

run_case() {
  name=$1
  shift
  output="$outdir/$name.ir"

  if "$compiler" "tests/ch6/$name.tig" >"$output" 2>&1; then
    :
  else
    echo "FAIL $name: compiler exited non-zero"
    failures=$((failures + 1))
    return
  fi

  for pattern in "$@"; do
    if grep -q "$pattern" "$output"; then
      :
    else
      echo "FAIL $name: expected IR pattern '$pattern'"
      failures=$((failures + 1))
    fi
  done
}

run_case 01_local_var "MEM" "MOVE"
run_case 02_two_escaping_locals "MEM" "BINOP(PLUS"
run_case 03_nested_static_link_var "CALL" "MEM"
run_case 04_nested_call_static_link "CALL" "BINOP(PLUS"
run_case 05_records_arrays_control "CALL" "malloc" "initArray" "CJUMP"

if [ "$failures" -ne 0 ]; then
  echo "$failures ch6 IR test check(s) failed"
  exit 1
fi

echo "all ch6 IR tests passed"
