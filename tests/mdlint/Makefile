.POSIX:
.SILENT:

AWK = awk
BUSYBOX_AWK = busybox awk
GNU_AWK = gawk
MAWK = mawk
ORIGINAL_AWK = original-awk

MDLINT = $$(command -v mdlint)
MDLINT_EXEC = _=$(MDLINT) $(AWK) -f $(MDLINT) --

AWK_VARIANTS = \
	"$(BUSYBOX_AWK)" \
	"$(GNU_AWK) --posix" \
	"$(GNU_AWK)" \
	"$(MAWK)" \
	"$(ORIGINAL_AWK)" \

# Verify that mdlint's output from processing "test.in" is identical to the
# contents of "test.out".
test:
	printf "%-28s" "$(AWK):"
	$(MDLINT_EXEC) --rule-names test.in | diff -u test.out /dev/fd/0
	echo " OK"

# Invoke the "test" target using various AWK interpreters to ensure that they
# all generate the same output when processing "test.in".
test-awk-portability:
	for awk in $(AWK_VARIANTS); do \
		$(MAKE) -s AWK="$$awk" test; \
	done

# Use GNU Awk's linter to detect problems in mdlint. Varying sets of options
# are used when invoking mdlint to exercise different code paths, increasing
# the amount of code coverage.
gawk-lint:
	printf "%-28s" "$@:"
	report_sprintf_lineno="$$( \
		sed -n -e '/^function report(/,/^}$$/!d' \
		       -e '/sprintf/=' $(MDLINT) \
	)"; \
	for options in \
		"--" \
		"--help" \
		"--rule-names" \
		"-H" \
		"-v -r broken_link_or_image" \
	  ; do \
		grep_status=0; \
		_=$(MDLINT) LC_ALL=C $(GNU_AWK) --lint --exec \
			$(MDLINT) $$options test.in 2>&1 >/dev/null \
		| grep -E -v \
			-e ":$$report_sprintf_lineno: warning: reference to uninitialized argument \`[b-f]'" \
			-e ":$$report_sprintf_lineno: warning: too many arguments supplied for format string" \
			-e "warning: assignment used in conditional context" \
			-e "warning: substr: length 0 is not >= 1" \
			-e "warning: substr: start index [0-9]+ is past end of string" \
		| grep . || grep_status="$$?"; \
		test "$$grep_status" -eq 1; \
	done
	echo " OK"

# Examine the source code, the documentation shown by "--help" and the expected
# test output to look for various problems with rule implementations reporting
# when:
# - A rule is not exercised by the test specimen, "test.in".
# - A rule is mentioned in the documentation shown by "--help" but does not
#   appear in the script's source code.
# - A rule appears in the script's source code but not in the output of
#   "--help".
check-rule-implementations:
	printf "%-28s" "$@:"
	implemented_rules="$$( \
		sed -n 's/^ *report("\([^"]*\).*/\1/p' $(MDLINT) \
	)"; \
	documented_rules="$$( \
		$(MDLINT_EXEC) --help | sed -n -e 's/^- \([a-z_]*\):.*/\1/p' \
	)"; \
	for implemented in $$implemented_rules; do \
		if ! grep -E -q "^[0-9]+: $$implemented:" test.out; then \
			test -n "$${status:-}" || echo && status=1; \
			printf "\055 %s: untested\n" "$$implemented"; \
		fi; \
		for documented in $$documented_rules; do \
			test "$$implemented" = "$$documented" && continue 2; \
		done; \
		test -n "$${status:-}" || echo && status=1; \
		printf "\055 %s: undocumented\n" "$$implemented"; \
	done; \
	for documented in $$documented_rules; do \
		for implemented in $$implemented_rules; do \
			test "$$implemented" = "$$documented" && continue 2; \
		done; \
		test -n "$${status:-}" || echo && status=1; \
		echo "\055 $$documented: not implemented"; \
	done; \
	test "$${status:=0}" -eq 0 && echo " OK" || exit "$$status"

# Execute all tests / behavior verification targets.
sanity:
	$(MAKE) test-awk-portability
	$(MAKE) gawk-lint
	$(MAKE) check-rule-implementations
