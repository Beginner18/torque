#include "license_pbs.h" /* See here for the software license */
#include <pbs_config.h>   /* the master config generated by configure */
#include <stdio.h>

#include <sys/types.h>
#include <string.h>
#include "portability.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "queue.h"
#include "pbs_job.h"
#include "log.h"

extern int LOGLEVEL;

int site_alt_router(
    
  svr_job *jobp,
  pbs_queue *qp,
  long retry_time)
  {

  char log_buf[LOCAL_LOG_BUF_SIZE];

  if (LOGLEVEL >= 7)
    {
    sprintf(log_buf, "%s", jobp->get_jobid());
    LOG_EVENT(PBSEVENT_JOB, PBS_EVENTCLASS_JOB, __func__, log_buf);
    }

  return (default_router(jobp, qp, retry_time));
  }
