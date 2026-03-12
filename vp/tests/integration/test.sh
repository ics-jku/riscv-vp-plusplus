#!/bin/sh
set -e

build() {
	[ $# -eq 1 ] || return 1

	# remove previously generated files
	git clean -fdX

	# build cmake system for requested target
	cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain/${1}.cmake .

	# build all test programs for the target
	make
}

export TESTVP="tiny32"
build rv32 >/dev/null
out="$(./run_tests.sh)"
printf "%s\n" "${out}" | sed 's/^/[RV32] /'

export TESTVP="tiny64"
build rv64 >/dev/null
out="$(./run_tests.sh)"
printf "%s\n" "${out}" | sed 's/^/[RV64] /'
