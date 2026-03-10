#ifndef VEXIUM_HTTP_MODULE_H
#define VEXIUM_HTTP_MODULE_H

#include "vm.h"

/* HTTP module for Vexium (basic implementation)
 * Provides: http_get, http_post
 */

/* Register HTTP module natives with the VM */
void vm_register_http_module(VM* vm);

#endif /* VEXIUM_HTTP_MODULE_H */
