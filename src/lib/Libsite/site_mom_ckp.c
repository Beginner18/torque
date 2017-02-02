#include "license_pbs.h" /* See here for the software license */
#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h> /* DEBUG */
/*
 * site_mom_pchk.c = a site modifible file
 *
 * Contains Pre and Post checkpoint stubs for MOM.
 */

/*
 * This is used only in mom and needs PBS_MOM defined in order to
 * have things from other .h files (such as struct task) be defined.
 * If UNSUPPORTED_MACH is defined, then we just fake it.
 */
#ifndef UNSUPPORTED_MACH
#define PBS_MOM
#endif

#include <sys/types.h>
#include <pwd.h>
#include "portability.h"
#include "list_link.h"
#include "server_limits.h"
#include "attribute.h"
#include "pbs_job.h"
#ifndef UNSUPPORTED_MACH
#include "mom_mach.h"
#include "mom_func.h"
#endif

/*
 * site_mom_postchk() - Post-checkpoint stub for MOM.
 * Called if checkpoint (on qhold,qterm) suceeded.
 *
 * Should return 0 If ok
 *      non-zero If not ok.
 */

int site_mom_postchk(

  mom_job *pjob,
  int      hold_type)

  {
  return 0;
  }

/*
 * site_mom_prerst() - Pre-restart stub for MOM.
 * Called just before restart is performed.
 *
 * Should return 0 if ok
 *  JOB_EXEC_FATAL1 for permanent error, abort job
 *  JOB_EXEC_RETRY for temporary problem, requeue job.
 */

int site_mom_prerst(

  mom_job *pjob)

  {
  return 0;
  }
