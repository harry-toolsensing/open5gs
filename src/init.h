#ifndef __INIT_H__
#define __INIT_H__

#include "core.h"
#include "core_errno.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

CORE_DECLARE(status_t) will_initialize(char *config_path, char *log_path);
CORE_DECLARE(status_t) did_initialize(char *config_path, char *log_path);
CORE_DECLARE_NONSTD(void) will_terminate(void);
CORE_DECLARE_NONSTD(void) did_terminate(void);

CORE_DECLARE(status_t) epc_initialize(char *config_path, char *log_path);
CORE_DECLARE_NONSTD(void) epc_terminate(void);

CORE_DECLARE(status_t) mme_initialize();
CORE_DECLARE_NONSTD(void) mme_terminate(void);

CORE_DECLARE(status_t) hss_initialize();
CORE_DECLARE_NONSTD(void) hss_terminate(void);

CORE_DECLARE(status_t) sgw_initialize();
CORE_DECLARE_NONSTD(void) sgw_terminate(void);

CORE_DECLARE(status_t) pgw_initialize();
CORE_DECLARE_NONSTD(void) pgw_terminate(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__NEXTEPC_H__ */
