#!/bin/sh
set -e

testdir="${TMPDIR:-/tmp}/integration-tests"
outfile="${testdir}/vp-out"
errfile="${testdir}/vp-err"

mkdir -p "${testdir}"
trap "rm -rf '${testdir}'" INT EXIT

for test in *; do
	[ -e "${test}/output" ] || continue

	name=${test##*/}
	printf "Running test case '%s': " "${name}"

	if [ "${name%%-*}" = "mc" ]; then
		vp="${TESTVP}-mc"
	else
		vp="${TESTVP}-vp"
	fi

	if [ -s "${test}/opts" ]; then
		set -- $(cat "${test}/opts")
	else
		set --
	fi

	(
		[ -r "${test}/input" ] && \
			exec < "${test}/input"

		"${vp}" "$@" "${test}/${name}" \
			1>"${outfile}.in" 2>"${errfile}"
	)

	# Post process $outfile to remove toolchain-specific output.
	if [ -x "${test}/post-process.sh" ]; then
		"${test}/post-process.sh" < "${outfile}.in" > "${outfile}"
	else
		cp "${outfile}.in" "${outfile}"
	fi

	if ! cmp -s "${outfile}" "${test}/output"; then
		printf "FAIL: Output didn't match.\n\n"
		diff -u "${outfile}" "${test}/output"
		exit 1
	fi

	printf "OK.\n"
done
