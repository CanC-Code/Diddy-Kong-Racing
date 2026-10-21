#ifndef PTR_SHIM_H
#define PTR_SHIM_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Register an allocation so we can detect its truncation later. */
void ptd_register_alloc(void *ptr, size_t size);

/* Check if a pointer looks like a truncated allocation.
 * Returns 1 if suspicious (and logs), 0 if okay. */
int ptd_suspect(void *ptr, const char *context);

#ifdef __cplusplus
}
#endif

#endif
