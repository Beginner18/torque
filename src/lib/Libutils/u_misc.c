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
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <set>

#include "utils.h"
#include "resource.h"

#ifndef MAX_CMD_ARGS
#define MAX_CMD_ARGS 10
#endif


const char *incompatible_l[] = { "nodes", "size", "mppwidth", "mem", "hostlist",
                                 "ncpus", "procs", "pvmem", "pmem", "vmem", "reqattr",
                                 "software", "geometry", "opsys", "tpn", "trl", NULL };

int    ArgC = 0;
char **ArgV = NULL;
char  *OriginalPath = NULL;
char  *OriginalCommand = NULL;

/**
 * Abort the program due to memory allocation failure.
 */
static void fail_nomem(void)
  {
  printf("ERROR:  Insufficient memory to save command line.  Shutting down.\n");
  exit(-1);
  }

/**
 * Locate the executable @a cmd in the supplied @a path.
 *
 * @pre-cond: cmd and path must be valid char *s
 * @param cmd   Command to locate (name or absolute path) (I)
 * @param path  Colon-delimited set of directories to search (e.g., $PATH) (I)
 * @return Absolute path to @a cmd (malloc'd), or NULL on failure.
 */
char *find_command(
    
  char *cmd,
  char *path)

  {
  char *token;
  char *saveptr = NULL;

  if ((!cmd) ||
      (!path))
    return(NULL);

  if (!(path = strdup(path)))
    fail_nomem();

  if (*cmd == '/')
    {
    /* Absolute path to command provided. */
    free(path);

    if (access(cmd, X_OK))
      return(NULL);
    else 
      return strdup(cmd);
    }
  else if (strchr(cmd, '/'))
    {
    char buff[PATH_MAX];
    size_t cwd_len;

    /* Relative path to command provided. */
    free(path);
    if (!getcwd(buff, sizeof(buff)))
      return(NULL);
    cwd_len = strlen(buff);
    if (cwd_len > sizeof(buff) - 2)
      return(NULL);
    
    strcat(buff, "/");
    cwd_len++;
    strncat(buff, cmd, sizeof(buff) - cwd_len - 1);

    return(strdup(buff));
    }

  token = strtok_r(path, ":;", &saveptr);
  while (token)
    {
    size_t len = strlen(token);
    char buff[PATH_MAX];

    if (len)
      {
      if (token[len - 1] == '/')
        {
        snprintf(buff, sizeof(buff), "%s%s", token, cmd);
        }
      else
        {
        snprintf(buff, sizeof(buff), "%s/%s", token, cmd);
        }
      if (!access(buff, X_OK))
        {
        free(path);
        return strdup(buff);
        }
      }
    token = strtok_r(NULL, ":;", &saveptr);
    }
  free(path);
  return NULL;
  }

/**
 * Store the program's @a argc and @a argv[] for later use.
 *
 * @param argc The @a argc value passed to main(). (I)
 * @param argv The @a argv[] array passed to main(). (I)
 */
void save_args(int argc, char **argv)
  {
  int i;
  char *env_ptr = NULL;

  ArgC = argc;
  if (!(ArgV = (char **) malloc(sizeof(char *) * (ArgC + 1))))
    fail_nomem();
  ArgV[ArgC] = 0;

  /* save argv and the path for later use */
  for (i = 0; i < ArgC; i++)
    {
    ArgV[i] = strdup(argv[i]);
    if (!ArgV[i]) fail_nomem();
    }

  /* save the path before we go into the background.  If we don't do this
   * we can't restart the server because the path will change */
  env_ptr = getenv("PATH");
  if (env_ptr != NULL)
    {
    if (!(OriginalPath = strdup(env_ptr)))
      fail_nomem();
    }
  else
    OriginalPath = NULL;

  OriginalCommand = ArgV[0];
  ArgV[0] = find_command(ArgV[0], OriginalPath);
  if (!ArgV[0])
    ArgV[0] = OriginalCommand;
  }



/*
 * add_range_to_string()
 *
 * Adds a range specified by begin and end to range_string
 *
 * @param range_string (O) - the string we're adding a range to
 * @param begin (I) - the first int in the range
 * @param end (I) - the last int in the range
 */

void add_range_to_string(

  std::string &range_string,
  int          begin,
  int          end)

  {
  char buf[MAXLINE];

  if (begin == end)
    {
    if (range_string.size() == 0)
      sprintf(buf, "%d", begin);
    else
      sprintf(buf, ",%d", begin);
    }
  else
    {
    if (range_string.size() == 0)
      sprintf(buf, "%d-%d", begin, end);
    else
      sprintf(buf, ",%d-%d", begin, end);
    }

  range_string += buf;
  } // END add_range_to_string()



/*
 * translate_vector_to_range_string()
 *
 * Takes the indices specified in indices and places them in a range string that holds the
 * form of %d[[-%d][,%d[-%d]]...]
 *
 * @param range_string (O) - the resulting string
 * @param indices (I) - the indices to place in the string
 */

void translate_vector_to_range_string(

  std::string            &range_string,
  const std::vector<int> &indices)

  {
  // range_string starts empty
  range_string.clear();

  if (indices.size() == 0)
    return;

  int first = indices[0];
  int prev = first;

  for (unsigned int i = 1; i < indices.size(); i++)
    {
    if (indices[i] == prev + 1)
      {
      // Still in a consecutive range
      prev = indices[i];
      }
    else
      {
      add_range_to_string(range_string, first, prev);

      first = prev = indices[i];
      }
    }

  // output final piece
  add_range_to_string(range_string, first, prev);
  } // END translate_vector_to_range_string()



/*
 * translate_range_string_to_vector()
 *
 * Takes a range string in the form of %d[[-%d][,%d[-%d]]...] and places each individual
 * int in a vector of ints.
 *
 * @param range_string (I) - the string specifying the range
 * @param indices (O) - the vector populated from range_string
 */

int translate_range_string_to_vector(

  const char       *range_string,
  std::vector<int> &indices)

  {
  char          *str = strdup(range_string);
  char          *ptr = str;
  int            prev = 0;
  int            curr;
  int            rc = PBSE_NONE;
  // use set to hold values added to vector
  //   since less expensive to check for duplicate entries
  std::set<int>  vector_values;

  while (is_whitespace(*ptr))
    ptr++;

  while (*ptr != '\0')
    {
    char *old_ptr = ptr;
    prev = strtol(ptr, &ptr, 10);

    if (ptr == old_ptr)
      {
      // This means *ptr wasn't numeric, error. break out to prevent an infinite loop
      rc = -1;
      break;
      }
    
    if (*ptr == '-')
      {
      ptr++;

      if (!isdigit(*ptr))
        {
        // Must be a digit at this point in time
        rc = -1;
        break;
        }

      curr = strtol(ptr, &ptr, 10);

      if (prev >= curr)
        {
        // invalid range
        rc = -1;
        break;
        }

      while (prev <= curr)
        {
        if (vector_values.insert(prev).second == false)
          {
          // duplicate entry
          rc = -1;
          break;
          }

        indices.push_back(prev);

        prev++;
        }

      if (rc != PBSE_NONE)
        break;

      while ((*ptr == ',') ||
          (is_whitespace(*ptr)))
        ptr++;
      }
    else
      {
      if (vector_values.insert(prev).second == false)
        {
        // duplicate entry
        rc = -1;
        break;
        }

      indices.push_back(prev);

      while ((*ptr == ',') ||
             (is_whitespace(*ptr)))
        ptr++;
      }
    }

  free(str);
  
  return(rc);
  } /* END translate_range_string_to_vector() */



/*
 * capture_until_close_character()
 */

void capture_until_close_character(

  char        **start,
  std::string  &storage,
  char          end)

  {
  if ((start == NULL) ||
      (*start == NULL))
    return;

  char *val = *start;
  char *ptr = strchr(val, end);

  // Make sure we found a close quote and this wasn't an empty string
  if ((ptr != NULL) &&
      (ptr != val))
    {
    storage = val;
    storage.erase(ptr - val);
    *start = ptr + 1; // add 1 to move past the character
    }
  else
    {
    // Make sure we aren't returning stale values
    storage.clear();
    }
  } // capture_until_close_character()


/* 
 * task_hosts_match()
 *
 * check for FQDN and short name to see if
 * host names match
 *
 * @param  task_host - first host name to match
 * @param  this_hostname - host name of physical host
 *
 */

bool task_hosts_match(
        
  const char *task_host, 
  const char *this_hostname)
 
  {
#ifdef NUMA_SUPPORT
  char *real_task_host = strdup(task_host);
  char *last_dash = NULL;

  if (real_task_host == NULL)
    return(false);

  last_dash = strrchr(real_task_host, '-');
  if (last_dash != NULL)
    *last_dash = '\0';
    
  if (strcmp(real_task_host, this_hostname))
    {
    /* see if the short name might match */
    char task_hostname[PBS_MAXHOSTNAME];
    char local_hostname[PBS_MAXHOSTNAME];
    char *dot_ptr;

    strcpy(task_hostname, real_task_host);
    strcpy(local_hostname, this_hostname);

    dot_ptr = strchr(task_hostname, '.');
    if (dot_ptr != NULL)
      *dot_ptr = '\0';

    dot_ptr = strchr(local_hostname, '.');
    if (dot_ptr != NULL)
      *dot_ptr = '\0';

    if (strcmp(task_hostname, local_hostname))
      {
      /* this task does not belong to this host. Go to the next one */
      return(false);
      }
    }


#else
  if (strcmp(task_host, this_hostname))
    {
    /* see if the short name might match */
    char task_hostname[PBS_MAXHOSTNAME];
    char local_hostname[PBS_MAXHOSTNAME];
    char *dot_ptr;

    strcpy(task_hostname, task_host);
    strcpy(local_hostname, this_hostname);

    dot_ptr = strchr(task_hostname, '.');
    if (dot_ptr != NULL)
      *dot_ptr = '\0';

    dot_ptr = strchr(local_hostname, '.');
    if (dot_ptr != NULL)
      *dot_ptr = '\0';

    if (strcmp(task_hostname, local_hostname))
      {
      /* this task does not belong to this host. Go to the next one */
      return(false);
      }
    }
#endif

  return(true);
  }



#ifdef PENABLE_LINUX_CGROUPS


/* 
 * have_incompatible_dash_l_resource
 *
 * Check to see if this is an incompatile -l resource
 * request for a -L syntax
 *
 * @param pjob  - the job structure we are working with
 *
 */

bool have_incompatible_dash_l_resource(

  pbs_attribute *pattr)

  {
  bool found_incompatible_resource = false;

  if ((pattr->at_flags & ATR_VFLAG_SET) &&
      (pattr->at_val.at_ptr != NULL))
    {
    std::vector<resource> *resources = (std::vector<resource> *)pattr->at_val.at_ptr;

    for (size_t j = 0; j < resources->size() && found_incompatible_resource == false; j++)
      {
      resource &r = resources->at(j);

      for (int i = 0; incompatible_l[i] != NULL; i++)
        {
        if (!strcmp(incompatible_l[i], r.rs_defin->rs_name))
          {
          found_incompatible_resource = true;
          break;
          }
        }
      }
    }

  return(found_incompatible_resource);
  } // END have_incompatible_dash_l_resource()


#endif /* PENABLE_LINUX_CGROUPS */
