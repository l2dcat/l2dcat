#include "test.h"

int l2dcat_test_failures;

int main(void) {
    test_config();
    test_input();
    test_models();
    test_shortcut();
    test_update();
    if (l2dcat_test_failures) {
        fprintf(stderr, "%d checks failed\n", l2dcat_test_failures);
        return 1;
    }
    puts("all core checks passed");
    return 0;
}
