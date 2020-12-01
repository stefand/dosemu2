/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef __EMS_H
#define __EMS_H

#define EMS_HELPER_EMM_INIT 0
/* increase this when ems.S is changed */
#define DOSEMU_EMS_DRIVER_VERSION 8
#define DOSEMU_EMS_DRIVER_MIN_VERSION 8

#define EMS_ERROR_DISABLED_IN_CONFIG 1
#define EMS_ERROR_VERSION_MISMATCH 2
#define EMS_ERROR_PFRAME_UNAVAIL 3
#define EMS_ERROR_ALREADY_INITIALIZED 4

#ifndef __ASSEMBLER__
/* export a few EMS functions to DPMI so it doesn't have to call interrupt */
int emm_allocate_handle(int pages_needed);
int emm_deallocate_handle(int handle);
int emm_save_handle_state(int handle);
int emm_restore_handle_state(int handle);
int emm_map_unmap_multi(const u_short *array, int handle, int map_len);
/* end of DPMI/EMS API */

void ems_init(void);
void ems_reset(void);

int emm_is_pframe_addr(dosaddr_t addr, uint32_t *size);
#endif

#endif
