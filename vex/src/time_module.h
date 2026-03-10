#ifndef VEXIUM_TIME_MODULE_H
#define VEXIUM_TIME_MODULE_H

#include "vm.h"

/* Time module for Vexium
 * Provides: time_now, time_format, time_sleep
 */

/* Register Time module natives with the VM */
void vm_register_time_module(VM* vm);

#endif /* VEXIUM_TIME_MODULE_H */
