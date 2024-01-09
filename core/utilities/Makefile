.POSIX:
.SILENT: all

UTILITIES = \
	bin/redundansi

all: $(UTILITIES)

# Dummy target used to ensure a recipe is always executed even if it is
# otherwise up to date.
ALWAYS_RUN:

# The combination of the "+" command prefix and "-n" is used to force make to
# display the command being executed even if "-s" is inherited from the
# invoking make.
$(UTILITIES): ALWAYS_RUN
	mkdir -p $(@D)
	source="$(@F).c"; \
	make=$$(sed -nf extract-recipe.sed "$$source"); \
	echo "$@: $$source; +$${make:?$@: recipe not found}" | $(MAKE) -n -f -
