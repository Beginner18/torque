/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/
#include <pbs_config.h>   /* the master config generated by configure */

/*
 * stat_job.c
 *
 * Functions which support the Status Job Batch Request.
 *
 * Included funtions are:
 * status_job()
 * status_attrib()
 */
#include <stdlib.h>
#include "libpbs.h"
#include <ctype.h>
#include <stdio.h>
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "server.h"
#include "queue.h"
#include "credential.h"
#include "batch_request.h"
#include "pbs_job.h"
#include "work_task.h"
#include "pbs_error.h"
#include "svrfunc.h"
#include "resource.h"
#include "svr_func.h" /* get_svr_attr_* */
#include "log.h"
#include "job_route.h" /* remove_procct */

extern int     svr_authorize_jobreq(struct batch_request *, job *);
int status_attrib(svrattrl *, attribute_def *, pbs_attribute *, int, int, tlist_head *, bool, int *, int);

/* Global Data Items: */

extern attribute_def job_attr_def[];

extern struct server server;





/**
 * status_job - Build the status reply for a single job.
 *
 * @see req_stat_job_step2() - parent
 * @see status_attrib() - child
 */

int status_job(

  job           *pjob, /* ptr to job to status */
  batch_request *preq,
  svrattrl      *pal, /* specific attributes to status */
  tlist_head    *pstathd, /* RETURN: head of list to append status to */
  bool           condensed,
  int           *bad) /* RETURN: index of first bad pbs_attribute */

  {
  struct brp_status *pstat;
  int                IsOwner = 0;
  bool               query_others = false;
  long               condensed_timeout = JOB_CONDENSED_TIMEOUT;

  /* Make sure procct is removed from the job 
     resource attributes */
  remove_procct(pjob);

  /* see if the client is authorized to status this job */
  if (svr_authorize_jobreq(preq, pjob) == 0)
    IsOwner = 1;

  get_svr_attr_b(SRV_ATR_query_others, &query_others);
  if (!query_others)
    {
    if (IsOwner == 0)
      {
      return(PBSE_PERM);
      }
    }
  
  get_svr_attr_l(SRV_ATR_job_full_report_time, &condensed_timeout);

  // if the job has been modified within the timeout, send the full output
  if ((condensed == true) &&
      (time(NULL) < pjob->ji_mod_time + condensed_timeout))
    condensed = false;

  /* allocate reply structure and fill in header portion */
  if ((pstat = (struct brp_status *)calloc(1, sizeof(struct brp_status))) == NULL)
    {
    return(PBSE_SYSTEM);
    }

  CLEAR_LINK(pstat->brp_stlink);

  pstat->brp_objtype = MGR_OBJ_JOB;

  strcpy(pstat->brp_objname, pjob->ji_qs.ji_jobid);

  CLEAR_HEAD(pstat->brp_attr);

  append_link(pstathd, &pstat->brp_stlink, pstat);

  /* add attributes to the status reply */
  *bad = 0;

  if (status_attrib(
        pal,
        job_attr_def,
        pjob->ji_wattr,
        JOB_ATR_LAST,
        preq->rq_perm,
        &pstat->brp_attr,
        condensed,
        bad,
        IsOwner))
    {
    return(PBSE_NOATTR);
    }
  else if (condensed == false)
    {
    pjob->encode_plugin_resource_usage(&pstat->brp_attr);
    }

  return (PBSE_NONE);
  }  /* END status_job() */



/* Is this dead code? It isn't called anywhere. */
int add_walltime_remaining(
   
  int             index,
  pbs_attribute  *pattr,
  tlist_head     *phead)

  {
  int            len = 0;
  char           buf[MAXPATHLEN+1];
  svrattrl      *pal;
  
  long           remaining = 0;
  time_t         time_now   = time(NULL);

  /* encode walltime remaining, this is custom because walltime 
   * remaining isn't an pbs_attribute */
  if (pattr[JOB_ATR_state].at_val.at_char != 'R')
    {
    /* only for running jobs, do nothing */
    return(PBSE_NONE);
    }

  resource_def *walltime_def = find_resc_def(svr_resc_def, "walltime", svr_resc_size);
  if (walltime_def != NULL)
    {
    resource *res = find_resc_entry(pattr + JOB_ATR_resource, walltime_def);
    if (res != NULL)
      {
      remaining = res->rs_value.at_val.at_long - (time_now - pattr[index].at_val.at_long);
      
      snprintf(buf,MAXPATHLEN,"%ld",remaining);
      
      len = strlen(buf);
      pal = attrlist_create("Walltime","Remaining",len+1);
      
      if (pal != NULL)
        {
        memcpy(pal->al_value,buf,len);
        pal->al_flags = ATR_VFLAG_SET;
        append_link(phead,&pal->al_link,pal);
        }
      }
    }

  return(PBSE_NONE);
  } /* END add_walltime_remaining() */


/*
 * include_in_status()
 *
 * Tells which attributes should be included in condensed output
 *
 * @param index -  the index of the attribute
 * @return - true if the attribute should be included in condensed output, false otherwise
 */
bool include_in_status(

  int index)

  {
  switch (index)
    {
    case JOB_ATR_jobname:
    case JOB_ATR_job_owner:
    case JOB_ATR_state:
    case JOB_ATR_resc_used:
    case JOB_ATR_in_queue:

      return(true);

    default:

      return(false);
    }

  return(true);
  } /* END include_in_status() */



/*
 * get_specific_attributes_status()
 *
 * Returns the specific attributes specified in pal instead of looping over 
 * all attributes.
 *
 * @param pal - the list of attributes to enode
 * @param padef - the attribute definition to work from
 * @param pattr - the attribute list we are encoding from
 * @param phead - the list we're encoding onto
 * @param priv - the privileges of the encoder
 * @param limit - the number of attributes in padef
 * @param bad - the attribute index that is bad. Output
 * @param IsOwner - TRUE if the encoder is an owner of this object, FALSE otherwise
 * @return - PBSE_NONE if successful, otherwise the appropriate pbs error code
 */
int get_specific_attributes_status(

  svrattrl      *pal,      /* I */
  attribute_def *padef,
  pbs_attribute *pattr,
  tlist_head    *phead,
  int            priv,
  int            limit,
  int           *bad,
  int            IsOwner)

  {
  int    nth = 0;
  char   log_buf[LOCAL_LOG_BUF_SIZE + 1];
  int    resc_access_perm;

  priv &= ATR_DFLAG_RDACC;  /* user-client privilege  */
  resc_access_perm = priv; 

  while (pal != NULL)
    {
    ++nth;

    int index = find_attr(padef, pal->al_name, limit);

    if (index < 0)
      {
      *bad = nth;

      snprintf(log_buf, LOCAL_LOG_BUF_SIZE, "Attribute %s not found. nth = %d", pal->al_name, nth);
      LOG_EVENT(PBSEVENT_JOB, PBS_EVENTCLASS_QUEUE, __func__, log_buf);

      /* FAILURE */
      return(PBSE_NOATTR);
      }

    if ((padef + index)->at_flags & priv)
      {
      if (!(((padef + index)->at_flags & ATR_DFLAG_PRIVR) && (IsOwner == 0)))
        {
        (padef + index)->at_encode(
          pattr + index,
          phead,
          (padef + index)->at_name,
          NULL,
          ATR_ENCODE_CLIENT,
          resc_access_perm);
        }
      }

    pal = (svrattrl *)GET_NEXT(pal->al_link);
    }

  if (padef == job_attr_def)
    {
    /* We want to return walltime remaining for all running jobs */
    if ((pattr + JOB_ATR_start_time)->at_flags & ATR_VFLAG_SET)
      add_walltime_remaining(JOB_ATR_start_time, pattr, phead);
    }
              
  /* SUCCESS */
  return(PBSE_NONE);
  } // END get_specific_attributes_values() 



/**
 * status_attrib - add each requested or all attributes to the status reply
 *
 *   Returns: 0 on success
 *           -1 on error (bad pbs_attribute), "bad" set to ordinal of pbs_attribute
 *
 * @see status_job() - parent
 * @see find_attr() - child
 * @see *->at_encode() - child
 */

int status_attrib(

  svrattrl      *pal,      /* I */
  attribute_def *padef,
  pbs_attribute *pattr,
  int            limit,
  int            priv,
  tlist_head    *phead,
  bool           condensed,
  int           *bad,
  int            IsOwner)  /* 0 == FALSE, 1 == TRUE */

  {
  int    index;
  int    resc_access_perm;
  
  priv &= ATR_DFLAG_RDACC;  /* user-client privilege  */
  resc_access_perm = priv; 

  /* for each pbs_attribute asked for or for all attributes, add to reply */

  if (pal != NULL)
    {
    /* client specified certain attributes */
    return(get_specific_attributes_status(pal, padef, pattr, phead, priv, limit, bad, IsOwner));
    }    /* END if (pal != NULL) */

  /* attrlist not specified, return all readable attributes */

  for (index = 0; index < limit; index++)
    {
    if ((condensed == true) &&
        (include_in_status(index) == false))
      continue;

    if (((padef + index)->at_flags & priv) &&
        !((padef + index)->at_flags & ATR_DFLAG_NOSTAT))
      {
      if (!(((padef + index)->at_flags & ATR_DFLAG_PRIVR) && (IsOwner == 0)))
        {
        (padef + index)->at_encode(
          pattr + index,
          phead,
          (padef + index)->at_name,
          NULL,
          ATR_ENCODE_CLIENT,
          resc_access_perm);

        /* add walltime remaining if started */
        if ((index == JOB_ATR_start_time) &&
            ((pattr + index)->at_flags & ATR_VFLAG_SET))
          add_walltime_remaining(index, pattr, phead);
        }
      }
    }    /* END for (index) */

  /* SUCCESS */
  return(PBSE_NONE);
  }  /* END status_attrib() */

/* END stat_job.c */

