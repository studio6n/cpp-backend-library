# Makefile for impetus.
#
# Five build variants, each a fully isolated subtree under build/<variant>/:
#   dev-memory   - ASan + UBSan, -O0, -g   (fast iteration, memory bugs)
#   dev-thread   - TSan + UBSan, -O0, -g   (fast iteration, race bugs)
#   alpha-memory - ASan + UBSan, -O2       (closer to PROD codegen)
#   alpha-thread - TSan + UBSan, -O2       (closer to PROD codegen)
#   prod         - no sanitizers, -O2      (what actually ships)
#
# Building any variant produces build/<variant>/lib/libimpetus.a, and also
# ensures the single shared build/include/ (every src/ header, mirrored)
# exists. That pair — build/<variant>/lib/ + build/include/ — is the
# complete, self-contained thing an external project links against — no
# separate install step, no clone of this repo required on their end.
#
# build/include/ is a literal copy of src/'s headers. It does not depend on
# which variant is being built — content is identical no matter which one
# triggers it — so it is built once, shared by all five variants, and is
# NOT part of any single variant's build/<variant>/ subtree or clean cycle.
# Only lib/libimpetus.a differs between variants (compiled from the same
# .cpp files with different flags).
#
# Pass BINARY=1 with any variant target (or `all`) to also build
# build/<variant>/bin/sandbox from src/sandbox/main.cpp, linked against that
# variant's library. Nothing in this Makefile runs it — testing is manual.
#
# No incremental builds: every variant target always cleans its own build
# directory first, then rebuilds from scratch. No make -j support.

CXX ?= g++
STD := -std=c++20

INCLUDES := -I src -I vendor
THREADFLAGS := -pthread

WARNFLAGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
             -Wnon-virtual-dtor -Wold-style-cast -Wformat=2 -Wcast-align -Werror

FRAMEPTR := -fno-omit-frame-pointer

BUILD_DIR := build

# ---- Source discovery: wildcard glob, sandbox/ excluded from the library ----
LIB_SOURCES    := $(shell find src -name "*.cpp" -not -path "src/sandbox/*")
VENDOR_SOURCES := $(shell find vendor -name "*.cpp" 2>/dev/null)
SANDBOX_SOURCE := src/sandbox/main.cpp

# ---- Per-variant flags ----
DEV_MEMORY_FLAGS   := -O0 -g $(FRAMEPTR) -fsanitize=address -fsanitize=undefined
DEV_THREAD_FLAGS   := -O0 -g $(FRAMEPTR) -fsanitize=thread -fsanitize=undefined
ALPHA_MEMORY_FLAGS := -O2 $(FRAMEPTR) -fsanitize=address -fsanitize=undefined
ALPHA_THREAD_FLAGS := -O2 $(FRAMEPTR) -fsanitize=thread -fsanitize=undefined
PROD_FLAGS         := -O2 $(FRAMEPTR)

VARIANTS := dev-memory dev-thread alpha-memory alpha-thread prod

.PHONY: help all clean install format lint \
        dev-memory dev-thread alpha-memory alpha-thread prod \
        clean-dev-memory clean-dev-thread clean-alpha-memory clean-alpha-thread clean-prod

# ---- Default target: usage only, builds nothing ----
help:
	@echo "impetus Makefile — available targets:"
	@echo "  make dev-memory    build DEV-MEMORY   (ASan+UBSan, -O0)"
	@echo "  make dev-thread    build DEV-THREAD   (TSan+UBSan, -O0)"
	@echo "  make alpha-memory  build ALPHA-MEMORY (ASan+UBSan, -O2)"
	@echo "  make alpha-thread  build ALPHA-THREAD (TSan+UBSan, -O2)"
	@echo "  make prod          build PROD         (no sanitizers, -O2)"
	@echo "  make all           build all 5 variants"
	@echo "  make clean         remove all build artifacts"
	@echo "  make install       (not supported)"
	@echo "  make format        reformat all src/ files with clang-format"
	@echo "  make lint          run clang-tidy + cppcheck (fatal on any finding)"
	@echo ""
	@echo "  Append BINARY=1 to any variant (or all) to also build"
	@echo "  build/<variant>/bin/sandbox from src/sandbox/main.cpp."

# =============================================================================
# Per-variant object/archive pattern rules
# =============================================================================

define VARIANT_RULES
build/$(1)/src/%.o: src/%.cpp
	@mkdir -p $$(dir $$@)
	$(CXX) $(STD) $(INCLUDES) $(THREADFLAGS) $(WARNFLAGS) $(2) -c $$< -o $$@

build/$(1)/vendor/%.o: vendor/%.cpp
	@mkdir -p $$(dir $$@)
	$(CXX) $(STD) $(INCLUDES) $(THREADFLAGS) $(WARNFLAGS) $(2) -c $$< -o $$@

build/$(1)/lib/libimpetus.a: $$(patsubst src/%.cpp,build/$(1)/src/%.o,$$(LIB_SOURCES)) \
                             $$(patsubst vendor/%.cpp,build/$(1)/vendor/%.o,$$(VENDOR_SOURCES))
	@mkdir -p $$(dir $$@)
	ar rcs $$@ $$^

clean-$(1):
	rm -rf $(BUILD_DIR)/$(1)

$(1): clean-$(1) build/$(1)/lib/libimpetus.a build/include $$(if $$(filter 1,$$(BINARY)),build/$(1)/bin/sandbox)

build/$(1)/bin/sandbox: $$(SANDBOX_SOURCE) build/$(1)/lib/libimpetus.a
	@mkdir -p $$(dir $$@)
	$(CXX) $(STD) $(INCLUDES) $(THREADFLAGS) $(WARNFLAGS) $(2) $$< \
	    -L build/$(1)/lib -limpetus -o $$@
endef

$(eval $(call VARIANT_RULES,dev-memory,$(DEV_MEMORY_FLAGS)))
$(eval $(call VARIANT_RULES,dev-thread,$(DEV_THREAD_FLAGS)))
$(eval $(call VARIANT_RULES,alpha-memory,$(ALPHA_MEMORY_FLAGS)))
$(eval $(call VARIANT_RULES,alpha-thread,$(ALPHA_THREAD_FLAGS)))
$(eval $(call VARIANT_RULES,prod,$(PROD_FLAGS)))

# ---- Shared header copy: identical across all variants.
# Depends on the actual header files, so within a single `make all` run
# (all 5 variants depending on this one target) Make builds it exactly
# once and reuses it for the rest of that run — it does not rebuild once
# per variant. It also correctly rebuilds if a header file actually
# changes before the next invocation. ----
build/include: $(shell find src -name "*.hpp")
	rm -rf $@
	mkdir -p $@
	cd src && find . -name "*.hpp" -print0 | tar --null -cf - -T - | tar -xf - -C ../$@

# =============================================================================
# Aggregate targets
# =============================================================================

all: dev-memory dev-thread alpha-memory alpha-thread prod

clean:
	rm -rf $(BUILD_DIR)

install:
	@echo "This is currently not supported"

# =============================================================================
# Tooling
# =============================================================================

format:
	find src -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

LINT_CHECKS := modernize-*,readability-*,bugprone-*,performance-*,cppcoreguidelines-*,concurrency-*

lint:
	clang-tidy -checks='$(LINT_CHECKS)' --warnings-as-errors='*' \
	    $(LIB_SOURCES) -- $(STD) $(INCLUDES)
	cppcheck --enable=warning,style,performance,portability,information,missingInclude \
	    $(STD) $(INCLUDES) --suppress=missingIncludeSystem --error-exitcode=1 src/
