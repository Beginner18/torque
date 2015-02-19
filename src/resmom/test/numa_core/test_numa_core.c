#include "machine.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <check.h>
#include "log.h"
#include "hwloc.h"
#include "pbs_error.h"



START_TEST(test_initializeCore)
  {
  int rc = -1;
  Core new_core;
  hwloc_topology_t topology = NULL;
  hwloc_obj_t core_obj;
  hwloc_obj_t prev = NULL;

  
  hwloc_topology_init(&topology);
  hwloc_topology_set_flags(topology, HWLOC_TOPOLOGY_FLAG_WHOLE_SYSTEM);
  hwloc_topology_load(topology);

  core_obj = hwloc_get_next_obj_by_type(topology, HWLOC_OBJ_SOCKET, prev);
  rc = new_core.initializeCore(core_obj, topology);
  fail_unless(rc == PBSE_NONE, "Failed to initialize cores");

  rc = new_core.getNumberOfProcessingUnits();
  fail_unless(rc != 0, "Failed to get number of processing units in core");
  }
END_TEST


START_TEST(test_displayAsString)
  {
  Core c;
  std::stringstream out;

  c.displayAsString(out);
  fail_unless(out.str() == "      Core -1 (1 threads)\n", out.str().c_str());
  }
END_TEST


Suite *numa_core_suite(void)
  {
  Suite *s = suite_create("numa_core test suite methods");
  TCase *tc_core = tcase_create("test_initializeCore");
  tcase_add_test(tc_core, test_initializeCore);
  suite_add_tcase(s, tc_core);
  
  tc_core = tcase_create("test_displayAsString");
  tcase_add_test(tc_core, test_displayAsString);
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
  sr = srunner_create(numa_core_suite());
  srunner_set_log(sr, "numa_core_suite.log");
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return(number_failed);
  }
