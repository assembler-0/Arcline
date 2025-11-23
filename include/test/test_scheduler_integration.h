#ifndef ARCLINE_TEST_SCHEDULER_INTEGRATION_H
#define ARCLINE_TEST_SCHEDULER_INTEGRATION_H

// Integration test functions
void test_timer_preemption(void);
void test_task_termination(void);
void test_killing_current_task(void);
void test_stress(void);

// Main test runner
void run_scheduler_integration_tests(void);

#endif // ARCLINE_TEST_SCHEDULER_INTEGRATION_H
