#!/bin/sh
set -e

if [ ! -d "${RISCV_VP_BASE}" ]; then
	printf "Directory '%s' with riscv-vp source does not exist\n" "${RISCV_VP_BASE}" 1>&2
	exit 1
fi

cmake -DCMAKE_BUILD_TYPE=Debug -DRISCV_VP_BASE="${RISCV_VP_BASE}" .
make

if ! command -v "valgrind" >/dev/null 2>&1; then
	echo "WARNING: Running parser tests without valgrind, install valgrind!" 1>&2
	exec ./run_tests.sh
fi

logfile="${TMPDIR:-/tmp}/libgdb-valgrind"
trap "rm -f '${logfile}' 2>/dev/null" INT EXIT

memleak=42
valgrind --exit-on-first-error=yes --error-exitcode=${memleak} \
	--leak-check=full --show-leak-kinds=all \
	--log-file="${logfile}" ./run_tests.sh || \
	(
		if [ $? -eq ${memleak} ]; then
			printf "\n  ===== Valgrind error report =====\n\n"
			cat "${logfile}" | sed 's/^/    /'
			exit ${memleak}
		fi
	)
