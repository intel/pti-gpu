/*==============================================================
 * Copyright (C) Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 * =============================================================
 *
 * One source for every synthetic shared library that libld_dependency_preload_test
 * builds; see AddPreloadStub() in CMakeLists.txt for the definitions it is compiled
 * with.
 *
 * Plain C on purpose: the fewer DT_NEEDED entries these libraries carry of their
 * own, the fewer ways they can fail for a reason the test did not set up.
 */

#if !defined(PRELOAD_STUB_NAME)
#error "PRELOAD_STUB_NAME must name this stub; use AddPreloadStub() to build it"
#endif

/* The project compiles with hidden visibility, and a hidden symbol is neither
 * linkable from another stub nor findable with dlsym. */
#define PRELOAD_EXPORT __attribute__((visibility("default")))

#define PRELOAD_CONCAT_(a, b) a##b
#define PRELOAD_CONCAT(a, b) PRELOAD_CONCAT_(a, b)
#define PRELOAD_SYMBOL(name) PRELOAD_CONCAT(PtiPreloadStub_, name)

PRELOAD_EXPORT int PRELOAD_SYMBOL(PRELOAD_STUB_NAME)(void) { return 0; }

#if defined(PRELOAD_STUB_NEEDS)
/* Calling into the other stub is what makes the linker record it as a DT_NEEDED
 * entry, the dependency the loader then has to resolve. */
PRELOAD_EXPORT int PRELOAD_SYMBOL(PRELOAD_STUB_NEEDS)(void);

PRELOAD_EXPORT int PRELOAD_CONCAT(PRELOAD_SYMBOL(PRELOAD_STUB_NAME), _Dependency)(void) {
  return PRELOAD_SYMBOL(PRELOAD_STUB_NEEDS)();
}
#endif

#if defined(PRELOAD_STUB_ALSO_NEEDS)
/* A second direct dependency lets a test prove that a successful first pre-load is
 * released when the Core library still cannot load its next dependency. */
PRELOAD_EXPORT int PRELOAD_SYMBOL(PRELOAD_STUB_ALSO_NEEDS)(void);

PRELOAD_EXPORT int PRELOAD_CONCAT(PRELOAD_SYMBOL(PRELOAD_STUB_NAME), _AlsoDependency)(void) {
  return PRELOAD_SYMBOL(PRELOAD_STUB_ALSO_NEEDS)();
}
#endif

#if defined(PRELOAD_STUB_UNDEFINED_SYMBOL)
/* Referenced here, defined nowhere: dlopen with RTLD_NOW fails on this with an
 * "undefined symbol" error rather than a missing-dependency one, which is a
 * failure no amount of pre-loading can fix. */
PRELOAD_EXPORT int PtiPreloadStubSymbolThatDoesNotExist(void);

PRELOAD_EXPORT int PRELOAD_CONCAT(PRELOAD_SYMBOL(PRELOAD_STUB_NAME), _Undefined)(void) {
  return PtiPreloadStubSymbolThatDoesNotExist();
}
#endif
