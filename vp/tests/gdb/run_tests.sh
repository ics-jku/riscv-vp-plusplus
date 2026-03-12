#!/bin/sh
set -e

GDB_DEBUG_PROG="${GDB_DEBUG_PROG:-gdb-multiarch}"
GDB_DEBUG_PORT="${GDB_DEBUG_PORT:-2342}"

VPFLAGS="--debug-mode --debug-port "${GDB_DEBUG_PORT}" --intercept-syscalls"

gdb_fail() {
	for file in "${testdir}/gdb-log" "${testdir}/gdb-out" "${testdir}/vp-out"; do
		printf "\n### Contents of '%s' follow ###\n\n" "${file##*/}"
		cat "${file}"
	done

	return 1
}

if ! command -v "${GDB_DEBUG_PROG}" >/dev/null 2>&1; then
	echo "GDB debug program '${GDB_DEBUG_PROG}' is not installed." 2>&1
	exit 1
fi

testdir="${TMPDIR:-/tmp}/gdb-tests"
outfile="${testdir}/gdb-log"

mkdir -p "${testdir}"
trap "rm -rf '${testdir}' ; kill -9 %1 2>/dev/null || true" INT EXIT

cat > "${testdir}/gdb-cmds.in" <<-EOF
	target remote :${GDB_DEBUG_PORT}
	set confirm off

	set height unlimited
	set width unlimited

	set logging file ${outfile}.in
	set logging overwrite 1
	set logging on
EOF

for test in *; do
	[ -e "${test}/output" ] || continue

	name=${test##*/}
	printf "Running test case '%s': " "${name}"

	if [ "${name%%-*}" = "mc" ]; then
		vp="${TESTVP}-mc"
	else
		vp="${TESTVP}-vp"
	fi
	("${vp}" ${VPFLAGS} "${test}/${name}" 1>"${testdir}/vp-out" 2>&1) &

	cat "${testdir}/gdb-cmds.in" "${test}/gdb-cmds" \
		> "${testdir}/gdb-cmds"
	"${GDB_DEBUG_PROG}" -q -x "${testdir}/gdb-cmds" "${test}/${name}" \
		1>"${testdir}/gdb-out" 2>&1 </dev/null

	# Post process GDB log file to remove toolchain-specific output.
	if [ -x "${test}/post-process.sh" ]; then
		"${test}/post-process.sh" < "${outfile}.in" > "${outfile}"
	else
		cp "${outfile}.in" "${outfile}"
	fi

	if ! cmp -s "${outfile}" "${test}/output"; then
		printf "FAIL: Output didn't match.\n\n"
		diff -u "${outfile}" "${test}/output" || gdb_fail
	fi

	printf "OK.\n"

	kill %1 2>/dev/null || true
	wait
done
