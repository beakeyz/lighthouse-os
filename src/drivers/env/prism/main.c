#include "drivers/env/prism/ast.h"
#include "drivers/env/prism/error.h"
#include "libk/flow/error.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <dev/driver.h>
#include <libk/string.h>

static const char* __sample_code = "mode driver\n"
                                   "\n"
                                   "func start[a:b]\n"
                                   "{\n"
                                   "}\n";

int _prism_init(driver_t* driver)
{
    prism_error_t error;
    prism_ast_t* test_ast;

    /* Make sure there is no error */
    init_prism_error(&error, NULL, NULL, NULL);

    /* Create an AST */
    test_ast = create_prism_ast(__sample_code, strlen(__sample_code), &error);

    /* Check for error */
    if (prism_is_error(&error))
        goto end_test;

    /* Debug */
    prism_ast_debug(test_ast);

end_test:
    if (prism_is_error(&error) && error.description)
        printf("[Prism] Error: %s\n", error.description);
    kernel_panic("PRISM TEST");
    return 0;
}

int _prism_exit()
{
    return 0;
}

EXPORT_DRIVER(_prism_backend) = {
    .m_name = "prism_backend",
    .m_type = DT_OTHER,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .f_init = _prism_init,
    .f_exit = _prism_exit,
};

EXPORT_DEPENDENCIES(deps) = {
    DRV_DEP_END
};
