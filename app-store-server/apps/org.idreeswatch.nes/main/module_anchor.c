/*
 * Force the optimized PIC module component into the shared object. The
 * official project_so helper compiles this tiny shim and links nes_core.a.
 */

extern const unsigned char idreeswatch_module;

__attribute__((used))
const void *idreeswatch_module_link_anchor = &idreeswatch_module;
