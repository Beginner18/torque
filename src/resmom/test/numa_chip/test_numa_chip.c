#include "machine.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "log.h"
#include "hwloc.h"
#include "pbs_error.h"
#include <sstream>

extern std::string my_placement_type;


START_TEST(test_initializeChip)
  {
  int rc = -1;
  int style;
  Chip new_chip;
  hwloc_topology_t topology = NULL;
  hwloc_obj_t chip_obj;
  hwloc_obj_t prev = NULL;

  
  hwloc_topology_init(&topology);
  hwloc_topology_set_flags(topology, HWLOC_TOPOLOGY_FLAG_WHOLE_SYSTEM);
  hwloc_topology_load(topology);

  style = get_hardware_style(topology);
  
  if (style == NON_NUMA)
    {
    hwloc_obj_t socket_obj;
    socket_obj = hwloc_get_next_obj_by_type(topology, HWLOC_OBJ_SOCKET, prev);
    if (socket_obj != NULL)
      {
      rc = new_chip.initializeNonNUMAChip(socket_obj, topology);
      fail_unless(rc==PBSE_NONE, "could not initialize non NUMA chip");
      }
    else
      {
      /* rc should be -1. This will flag the error for us */
      fail_unless(rc == PBSE_NONE, "Could not get chip object from hwloc");
      }
    }
  else if (style == INTEL || style == AMD)
    {
   chip_obj = hwloc_get_next_obj_by_type(topology, HWLOC_OBJ_NODE, prev);
    if (chip_obj != NULL)
      {
      rc = new_chip.initializeChip(chip_obj, topology);
      fail_unless(rc==PBSE_NONE, "could not initialize  NUMA chip");
      }
    else
      {
      /* rc should be -1. This will flag the error for us */
      fail_unless(rc == PBSE_NONE, "Could not get chip object from hwloc");
      }
    }
  else
    fail_unless(style==AMD, "Could not get hardware style");



  rc = new_chip.getMemory();
  fail_unless(rc != 0, "failed to get chip memory");

  rc = new_chip.getTotalCores();
  fail_unless(rc != 0, "failed to get chip total cores");

  rc = new_chip.getTotalThreads();
  fail_unless(rc != 0, "failed to get chip total Threads");

  rc = new_chip.getAvailableCores();
  fail_unless(rc != 0, "failed to get chip available cores");

  rc = new_chip.getAvailableThreads();
  fail_unless(rc != 0, "failed to get chip available Threads");


  hwloc_topology_destroy(topology);

  }
END_TEST


START_TEST(test_displayAsString)
  {
  std::stringstream out;
  Chip c;

  c.setMemory(2);
  c.setId(0);
  c.displayAsString(out);
  fail_unless(out.str() == "    Chip 0 (2KB)\n", out.str().c_str());
  }
END_TEST


START_TEST(test_how_many_tasks_fit)
  {
  req r;
  r.set_value("lprocs", "2");
  r.set_value("memory", "1kb");

  Chip c;
  c.setThreads(12);
  c.setMemory(20);
  c.setChipAvailable(true);

  // test against threads
  int tasks = c.how_many_tasks_fit(r);
  fail_unless(tasks == 6, "%d tasks fit, expected 6", tasks);

  // Now memory should be the limiting factor
  c.setMemory(5);
  tasks = c.how_many_tasks_fit(r);
  fail_unless(tasks == 5, "%d tasks fit, expected 5", tasks);

  my_placement_type = use_cores;
  // Cores are currently 0
  tasks = c.how_many_tasks_fit(r);
  fail_unless(tasks == 0, "%d tasks fit, expected 0", tasks);

  c.setCores(2);
  tasks = c.how_many_tasks_fit(r);
  fail_unless(tasks == 1, "%d tasks fit, expected 0", tasks);

  // make sure that we can handle a request without memory
  req r2;
  r2.set_value("lprocs", "2");
  c.setCores(10);
  fail_unless(c.how_many_tasks_fit(r2) == 5);
  

  }
END_TEST


START_TEST(test_place_and_free_task)
  {
  const char *jobid = "1.napali";
  req r;
  r.set_value("lprocs", "2");
  r.set_value("memory", "1kb");

  allocation a(jobid);

  Chip c;
  c.setId(0);
  c.setThreads(24);
  c.setCores(12);
  c.setMemory(6);
  c.setChipAvailable(true);
  c.make_core(0);
  c.make_core(1);
  c.make_core(2);
  c.make_core(3);
  c.make_core(4);
  c.make_core(5);
  c.make_core(6);
  c.make_core(7);
  c.make_core(8);
  c.make_core(9);
  c.make_core(10);
  c.make_core(11);
 
  my_placement_type = use_cores;
  // fill the node's memory
  int tasks = c.place_task(jobid, r, a, 6);
  fail_unless(tasks == 6, "Placed only %d tasks, expected 6", tasks);
  fail_unless(a.mem_indices.size() > 0);
  fail_unless(a.mem_indices[0] == 0);

  // Memory should be full now
  tasks = c.place_task(jobid, r, a, 6);
  fail_unless(tasks == 0, "Placed %d tasks, expected 0", tasks);

  // make sure we can free and replace
  c.free_task(jobid);
  tasks = c.place_task(jobid, r, a, 6);
  fail_unless(tasks == 6, "Placed only %d tasks, expected 6", tasks);
  c.free_task(jobid);
  fail_unless(c.getAvailableCores() == 12);
  fail_unless(c.getAvailableThreads() == 24);
  
  // Now place by thread
  my_placement_type = "";
  c.setMemory(40);

  // Fill up the threads with multiple jobs
  const char *jobid2 = "2.napali";
  const char *jobid3 = "3.napali";
  tasks = c.place_task(jobid, r, a, 6);
  fail_unless(tasks == 6, "Expected 6 but placed %d", tasks);
  tasks = c.place_task(jobid2, r, a, 3);
  fail_unless(tasks == 3, "Expected 3 but placed %d", tasks);
  tasks = c.place_task(jobid3, r, a, 3);
  fail_unless(tasks == 3, "Expected 3 but placed %d", tasks);
  
  // Make sure we're full
  fail_unless(c.getAvailableCores() == 0);
  fail_unless(c.getAvailableThreads() == 0);
  tasks = c.place_task(jobid3, r, a, 1);
  fail_unless(tasks == 0);

  // Make sure we free correctly
  fail_unless(c.free_task(jobid3) == false);
  fail_unless(c.getAvailableCores() == 3);
  int threads = c.getAvailableThreads();
  fail_unless(threads == 6, "Expected 6 threads but got %d", threads);

  // Make sure a repeat free does nothing
  fail_unless(c.free_task(jobid3) == false);
  fail_unless(c.getAvailableCores() == 3);
  fail_unless(c.getAvailableThreads() == 6);
  
  fail_unless(c.free_task(jobid2) == false);
  fail_unless(c.getAvailableCores() == 6);
  fail_unless(c.getAvailableThreads() == 12);
  
  // We should be free now
  fail_unless(c.free_task(jobid) == true);
  fail_unless(c.getAvailableCores() == 12);
  fail_unless(c.getAvailableThreads() == 24);
  }
END_TEST


Suite *numa_socket_suite(void)
  {
  Suite *s = suite_create("numa_socket test suite methods");
  TCase *tc_core = tcase_create("test_initializeChip");
  tcase_add_test(tc_core, test_initializeChip);
  tcase_add_test(tc_core, test_how_many_tasks_fit);
  suite_add_tcase(s, tc_core);
  
  tc_core = tcase_create("test_displayAsString");
  tcase_add_test(tc_core, test_displayAsString);
  tcase_add_test(tc_core, test_place_and_free_task);
  suite_add_tcase(s, tc_core);
  
  return(s);
  }

void rundebug()
  {
  }

int main(void)
  {
  int number_failed = 0;
  SRunner *sr = NULL;
  rundebug();
  sr = srunner_create(numa_socket_suite());
  srunner_set_log(sr, "numa_socket_suite.log");
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return(number_failed);
  }
