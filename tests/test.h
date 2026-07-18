#ifndef L2DCAT_TEST_H
#define L2DCAT_TEST_H

#include <stdio.h>

extern int l2dcat_test_failures;
#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #value); \
    l2dcat_test_failures++; \
} } while (0)

void test_config(void);
void test_input(void);
void test_models(void);
void test_shortcut(void);
void test_update(void);

#endif
