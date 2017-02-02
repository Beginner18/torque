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

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "pbs_ifl.h"
#include "log.h"
#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "pbs_error.h"
#include "pbs_helper.h"

/*
 * This file contains functions for manipulating attributes of type
 * resource
 *
 * A "resource" is similiar to an pbs_attribute but with two levels of
 * names.  The first name is the pbs_attribute name, e.g. "resource-list",
 * the second name is the resource name, e.g. "mem".
 *
 * Each resource_def has functions for:
 * Decoding the value string to the internal representation.
 * Encoding the internal pbs_attribute to external form
 * Setting the value by =, + or - operators.
 * Comparing a (decoded) value with the pbs_attribute value.
 * freeing the resource value space (if extra memory is allocated)
 *
 * Some or all of the functions for an resource type may be shared with
 * other resource types or even attributes.
 *
 * The prototypes are declared in "attribute.h", also see resource.h
 *
 * ----------------------------------------------------------------------------
 * pbs_Attribute functions for attributes with value type resource
 * ----------------------------------------------------------------------------
 */

/* External Global Items */

int comp_resc_gt; /* count of resources compared > */
int comp_resc_eq; /* count of resources compared = */
int comp_resc_lt; /* count of resources compared < */
int comp_resc_nc; /* count of resources not compared */



/*
 * decode_resc - decode a "pbs_attribute name/resource name/value" triplet into
 *          a resource type pbs_attribute
 *
 * Returns: 0 if ok,
 *  >0 error number if error,
 *  *patr members set
 */

int decode_resc(

  pbs_attribute *patr,  /* Modified on Return */
  const char    *name,  /* pbs_attribute name */
  const char    *rescn, /* I resource name - is used here */
  const char    *val,   /* resource value */
  int            perm)  /* access permissions */

  {
  resource *prsc;
  resource_def *prdef;
  int   rc = 0;
  int   rv;

  if (patr == NULL)
    {
    return(PBSE_INTERNAL);
    }

  if (rescn == NULL)
    {
    return(PBSE_UNKRESC);
    }

  if (!(patr->at_flags & ATR_VFLAG_SET))
    {
    if (patr->at_val.at_ptr != NULL)
      {
      std::vector<resource> *resources = (std::vector<resource> *)patr->at_val.at_ptr;
      resources->clear();
      }
    }

  prdef = find_resc_def(svr_resc_def, rescn, svr_resc_size);

  if (prdef == NULL)
    {
    /*
     * didn't find resource with matching name, use unknown;
     * but return PBSE_UNKRESC in case caller doesn`t wish to
     * accept unknown resources
     */

    rc = PBSE_UNKRESC;

    prdef = svr_resc_def + (svr_resc_size - 1);
    }

  prsc = find_resc_entry(patr, prdef);

  if (prsc == NULL) /* no current resource entry, add it */
    {
    if ((prsc = add_resource_entry(patr, prdef)) == NULL)
      {
      return(PBSE_SYSTEM);
      }
    }

  /* note special use of ATR_DFLAG_ACCESS, see server/attr_recov() */

  if (((prsc->rs_defin->rs_flags & perm & ATR_DFLAG_WRACC) == 0) &&
      (perm != ATR_DFLAG_ACCESS))
    {
    return(PBSE_ATTRRO);
    }

  patr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;

  rv = prdef->rs_decode(&prsc->rs_value, name, rescn, val, perm);

  if (rv == 0)
    {
    /* FAILURE */

    return(rc);
    }

  /* SUCCESS */

  return(rv);
  } // END decode_resc()



int encode_resc_from_vector(

  std::vector<resource> &resources,
  tlist_head            *phead,
  const char            *atname,
  int                    mode,
  int                    ac_perm)

  {
  int total = 0;
  int rc;
  
  for (size_t i = 0; i < resources.size(); i++)
    {
    /*
     * encode if sending to client or MOM with permission
     * encode if saving and not default value
     * encode if sending to server and not default and have permission
     */
    int perm = resources[i].rs_defin->rs_flags & ac_perm ;
    int dflt = resources[i].rs_value.at_flags & ATR_VFLAG_DEFLT;

    if (((mode == ATR_ENCODE_CLIENT) && perm)  ||
        ((mode == ATR_ENCODE_MOM) && perm)     ||
        ((mode == ATR_ENCODE_SAVE) && (dflt == 0))  ||
        ((mode == ATR_ENCODE_SVR)  && (dflt == 0) && perm))
      {
      const char *rsname = resources[i].rs_defin->rs_name;
      rc = resources[i].rs_defin->rs_encode(&resources[i].rs_value, phead, atname, rsname, mode,
                                            ac_perm);

      if (rc < 0)
        return (rc);

      total += rc;
      }
    }
  
  return(total);
  } // END encode_resc_from_vector()



/*
 * encode_resc - encode attr of type ATR_TYPE_RESR into attr_extern form
 *
 * Here we are a little different from the typical pbs_attribute.  Most have a
 * single value to be encoded.  But resource pbs_attribute may have a whole bunch.
 * First get the name of the parent pbs_attribute (typically "resource-list").
 * Then for each resource in the list, call the individual resource decode
 * routine with "aname" set to the parent pbs_attribute name.
 *
 * If mode is either ATR_ENCODE_SAVE or ATR_ENCODE_SVR, then any resource
 * currently set to the default value is not encoded.   This allows it to be
 * reset if the default changes or it is moved.
 *
 * If the mode is ATR_ENCODE_CLIENT or ATR_ENCODE_MOM, the client permission
 * passed in ac_perm is checked against each
 * definition.  This allows a resource by resource access setting, not just
 * on the pbs_attribute.
 *
 * Returns: >0 if ok
 *   =0 if no value to encode, no entries added to list
 *   <0 if some resource entry had an encode error.
 */

int encode_resc(

  pbs_attribute  *attr,    /* ptr to pbs_attribute to encode */
  tlist_head     *phead,   /* head of attrlist list */
  const char    *atname,  /* pbs_attribute name */
  const char    *rsname,  /* resource name, null on call */
  int             mode,    /* encode mode */
  int             ac_perm) /* access permissions */

  {
  if (attr == NULL)
    {
    return(-1);
    }

  if ((!(attr->at_flags & ATR_VFLAG_SET)) ||
      (attr->at_val.at_ptr == NULL))
    {
    return(0); /* no resources at all */
    }

  /* ok now do each separate resource */
  std::vector<resource> &resources = *((std::vector<resource> *)attr->at_val.at_ptr);

  return(encode_resc_from_vector(resources, phead, atname, mode, ac_perm));
  } // END encode_resc()



/*
 * set_resc - set value of pbs_attribute of type ATR_TYPE_RESR to another
 *
 * For each resource in the list headed by the "new" pbs_attribute,
 * the correspondingly name resource in the list headed by "old"
 * is modified.
 *
 * The mapping of the operations incr and decr depend on the type
 * of each individual resource.
 * Returns: 0 if ok
 *  >0 if error
 */

int set_resc(

  pbs_attribute *old,
  pbs_attribute *new_attr,
  enum batch_op  op)

  {
  enum batch_op local_op;
  resource *oldresc;
  int   rc;

  if (old == NULL || new_attr == NULL)
    return(PBSE_BAD_PARAMETER);

  if (new_attr->at_val.at_ptr == NULL)
    return(PBSE_NONE);

  std::vector<resource> *new_resc = (std::vector<resource> *)new_attr->at_val.at_ptr;

  for (size_t i = 0; i < new_resc->size(); i++)
    {
    resource &r = new_resc->at(i);

    local_op = op;

    /* search for old that has same definition as new */

    oldresc = find_resc_entry(old, r.rs_defin);

    if (oldresc == NULL)
      {
      /* add new resource to list */

      oldresc = add_resource_entry(old, r.rs_defin);

      if (oldresc == NULL)
        {
        return(PBSE_SYSTEM);
        }
      }

    /*
     * unlike other attributes, resources can be "unset"
     * if new is "set" to a value, the old one is set to that
     * value; if the new resource is unset (no value), then the
     * old resource is unset by freeing it.
     */

    if (r.rs_value.at_flags & ATR_VFLAG_SET)
      {
      /* call resource type dependent  set routine */
      if ((rc = oldresc->rs_defin->rs_set(&oldresc->rs_value, &r.rs_value, local_op)) != 0)
        return (rc);
      }
    else
      {
      oldresc->rs_defin->rs_free(&oldresc->rs_value);
      }
    }

  old->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;

  return(0);
  } // END set_resc()



/*
 * __comp_work__()
 *
 * Performs the work of comparing two attributes. These attributes have already been 
 * checked as valid for comparisions
 */

int __comp_work__(

  pbs_attribute *attr,
  pbs_attribute *with,
  int           &greater_than,
  int           &less_than,
  bool           check_default,
  bool           compare_forwards,
  char          *EMsg)

  {
  std::vector<resource> *with_resc = (std::vector<resource> *)with->at_val.at_ptr;

  for (size_t i = 0; i < with_resc->size(); i++)
    {
    resource &r = with_resc->at(i);

    if ((r.rs_value.at_flags & ATR_VFLAG_SET) &&
        ((check_default == true) ||
         ((r.rs_value.at_flags & ATR_VFLAG_DEFLT) == 0)))
      {
      resource *atresc = find_resc_entry(attr, r.rs_defin);

      if (atresc != NULL)
        {
        if (atresc->rs_value.at_flags & ATR_VFLAG_SET)
          {
          int rc;

          if (compare_forwards)
            rc = atresc->rs_defin->rs_comp(&atresc->rs_value, &r.rs_value);
          else
            rc = atresc->rs_defin->rs_comp(&r.rs_value, &atresc->rs_value);

          if (rc > 0)
            {
            if ((EMsg != NULL) &&
                (EMsg[0] == '\0'))
              sprintf(EMsg, "Cannot satisfy queue min %s requirement", r.rs_defin->rs_name);

            greater_than++;
            }
          else if (rc < 0)
            {
            less_than++;
            }
          }
        }
      }
    }  /* END while() */

  return(0);
  } // END __comp_work__()



/*
 * comp_resc - compare two attributes of type ATR_TYPE_RESR
 *
 *      DANGER Will Robinson, DANGER
 *
 *      As you can see from the returns, this is different from the
 *      at_comp model...
 *
 *      Returns: 0 if compare successful:
 *                 sets comp_resc_gt to count of "greater than" compares
 *                              attr > with
 *                 sets comp_resc_eq to count of "equal to" compares
 *                              attr == with
 *                 sets comp_resc_lt to count of "less than" compares
 *                              attr < with
 *              -1 if error
 */

/* NOTE:  if IsQueueCentric is 0, enforce for every job attr set,
          if IsQueueCentric is 1, enforce for every queue attr set
          old default behavior was '1' */

int comp_resc(

  pbs_attribute *attr,  /* I queue's min/max attributes */
  pbs_attribute *with)  /* I job's current requirements/attributes */

  {
  comp_resc_gt = 0;
  comp_resc_eq = 0;
  comp_resc_lt = 0;
  comp_resc_nc = 0;

  if ((attr == NULL) || (with == NULL))
    {
    /* FAILURE */
    return(-1);
    }

  if ((attr->at_val.at_ptr == NULL) ||
      (with->at_val.at_ptr == NULL))
    {
    return(0);
    }

  /* comparison is job centric */

  /* NOTE:  this check only enforces attributes if the job specifies the
            pbs_attribute.  If the queue has a min requirement of resource X
            and the job has no value set for this resource, this routine
            will not trigger comp_resc_lt */
  __comp_work__(attr, with, comp_resc_gt, comp_resc_lt, false, true, NULL);

  return(0);
  }  /* END comp_resc() */



/*
 * comp_resc2 - compare two attributes of type ATR_TYPE_RESR
 *
 * DANGER Will Robinson, DANGER
 *
 * As you can see from the returns, this is different from the
 * at_comp model...
 *
 * Returns: 0 if compare successful:
 *     sets comp_resc_gt to count of "greater than" compares
 *    attr > with
 *     sets comp_resc_eq to count of "equal to" compares
 *    attr == with
 *     sets comp_resc_lt to count of "less than" compares
 *    attr < with
 *  -1 if error
 */

/* NOTE:  if IsQueueCentric is 0, enforce for every job attr set,
          if IsQueueCentric is 1, enforce for every queue attr set
          old default behavior was '1' */

int comp_resc2(

  pbs_attribute      *queue_attr,     /* I queue's min/max attributes */
  pbs_attribute      *job_attr,       /* I job's current requirements/attributes */
  int                 IsQueueCentric, /* I */
  char               *EMsg,           /* O (optional,minsize=1024) */
  enum compare_types  type)           /* I type of comparison to detect */

  {
  int       comp_ret = 0;
  int       local_gt = 0;
  int       local_lt = 0;

  comp_resc_gt = 0;
  comp_resc_eq = 0;
  comp_resc_lt = 0;
  comp_resc_nc = 0;

  if (EMsg != NULL)
    EMsg[0] = '\0';

  if ((queue_attr == NULL) || (job_attr == NULL))
    {
    /* FAILURE */

    return(-1);
    }
  
  if ((queue_attr->at_val.at_ptr == NULL) ||
      (job_attr->at_val.at_ptr == NULL))
    {
    return(comp_ret);
    }

  if (IsQueueCentric == 1)
    {
    /* comparison is queue centric */
    __comp_work__(queue_attr, job_attr, local_gt, local_lt, true, true, EMsg);
    }
  else
    {
    /* comparison is job centric */

    /* NOTE:  this check only enforces attributes if the job specifies the
              pbs_attribute.  If the queue has a min requirement of resource X
              and the job has no value set for this resource, this routine
              will not trigger comp_resc_lt */
    __comp_work__(job_attr, queue_attr, local_gt, local_lt, false, false, EMsg);
    }

  if (type == GREATER)
    comp_ret = local_gt;
  else if (type == LESS)
    comp_ret = local_lt;

  comp_resc_gt = local_gt;
  comp_resc_lt = local_lt;

  return(comp_ret);
  }  /* END comp_resc2() */



/*
 * free_resc - free space associated with pbs_attribute value
 *
 * For each entry in the resource list, the entry is delinked,
 * the resource entry value space freed (by calling the resource
 * free routine), and then the resource structure is freed.
 */

void free_resc(

  pbs_attribute *pattr)

  {
  std::vector<resource> *resources = (std::vector<resource> *)pattr->at_val.at_ptr;

  if (resources != NULL)
    {
    for (size_t i = 0; i < resources->size(); i++)
      {
      resource &r = resources->at(i);
      r.rs_defin->rs_free(&r.rs_value);
      }

    delete resources;
    }

  pattr->at_val.at_ptr = NULL;
  pattr->at_flags &= ~ATR_VFLAG_SET;

  return;
  }  /* END free_resc() */





/*
 * find_resc_def - find the resource_def structure for a resource with
 * a given name
 *
 * Returns: pointer to the structure or NULL
 */

resource_def *find_resc_def(

  resource_def *rscdf, /* address of array of resource_def structs */
  const char  *name, /* name of resource */
  int           limit) /* number of members in resource_def array */

  {
  while (limit--)
    {
    if (!strcmp(rscdf->rs_name, name))
      {
      /* SUCCESS */

      return(rscdf);
      }

    rscdf++;
    }

  return(NULL);
  }  /* END find_resc_def() */



resource *find_resc_from_vector(

  std::vector<resource> *resources,
  resource_def          *rscdf)

  {
  resource *pr = NULL;

  for (size_t i = 0; i < resources->size(); i++)
    {
    resource &r = resources->at(i);
    if (!strcmp(r.rs_defin->rs_name, rscdf->rs_name))
      {
      pr = &r;
      break;
      }
    }

  return(pr);
  } // find_resc_from_vector()



/*
 * find_resc_entry - find a resource (value) entry in a list headed in an
 * an pbs_attribute that points to the specified resource_def structure
 *
 * Returns: pointer to struct resource or NULL
 */

resource *find_resc_entry(

  pbs_attribute *pattr,  /* I */
  resource_def  *rscdf)  /* I */

  {
  if ((pattr == NULL) ||
      (pattr->at_val.at_ptr == NULL))
    return(NULL);

  std::vector<resource> *resources = (std::vector<resource> *)pattr->at_val.at_ptr;

  return(find_resc_from_vector(resources, rscdf));
  }  /* END find_resc_entry() */




/*
 * add_resource_entry - add and "unset" entry for a resource type to a
 * list headed in an pbs_attribute.  Just for later displaying, the
 * resource list is maintained in an alphabetic order.
 * The parent pbs_attribute is marked with ATR_VFLAG_SET and ATR_VFLAG_MODIFY
 *
 * Returns: pointer to the newly added entry or NULL if unable
 *   to add it (calloc failed).  If the resource already
 *   exists (it shouldn't) then that one is returned.
 */

resource *add_resource_entry(

  pbs_attribute    *pattr,
  resource_def     *prdef)

  {
  resource  new_resource;

  if (pattr->at_val.at_ptr == NULL)
    pattr->at_val.at_ptr = new std::vector<resource>();

  std::vector<resource> *resources = (std::vector<resource> *)pattr->at_val.at_ptr;

  for (size_t i = 0; i < resources->size(); i++)
    {
    resource &r = resources->at(i);

    if (!strcmp(r.rs_defin->rs_name, prdef->rs_name))
      {
      return(&r);
      }
    }

  new_resource.rs_defin = prdef;
  new_resource.rs_value.at_type = prdef->rs_type;
  new_resource.rs_value.at_flags = 0;
  prdef->rs_free(&new_resource.rs_value);

  resources->push_back(new_resource);
  pattr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;
  
  // Return the element we just added
  resource &r = resources->at(resources->size() - 1);

  return(&r);
  } /* END add_resource_entry() */



/*
 * action_resc - the at_action for the resource_list pbs_attribute
 * For each resource in the list, if it has its own action routine,
 * call it.
 */

int action_resc(

  pbs_attribute *pattr,
  void          * UNUSED(pobject),
  int            actmode)

  {
  if (pattr->at_val.at_ptr != NULL)
    {
    std::vector<resource> *resources = (std::vector<resource> *)pattr->at_val.at_ptr;

    for (size_t i = 0; i < resources->size(); i++)
      {
      resource &r = resources->at(i);
      
      if ((r.rs_value.at_flags & ATR_VFLAG_MODIFY) &&
          (r.rs_defin->rs_action))
        r.rs_defin->rs_action(&r, pattr, actmode);

      resources->at(i).rs_value.at_flags &= ~ATR_VFLAG_MODIFY;
      }
    }

  return(PBSE_NONE);
  } // END action_resc()

