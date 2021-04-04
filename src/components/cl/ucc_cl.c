/**
 * Copyright (C) Mellanox Technologies Ltd. 2020.  ALL RIGHTS RESERVED.
 *
 * See file LICENSE for terms.
 */

#include "ucc_cl.h"
#include "utils/ucc_log.h"
#include "utils/ucc_malloc.h"

ucc_config_field_t ucc_cl_lib_config_table[] = {
    {"", "", NULL, ucc_offsetof(ucc_cl_lib_config_t, super),
     UCC_CONFIG_TYPE_TABLE(ucc_base_config_table)},

    {NULL}
};

ucc_config_field_t ucc_cl_context_config_table[] = {
    {NULL}
};

const char *ucc_cl_names[] = {
    [UCC_CL_BASIC] = "basic",
    [UCC_CL_ALL]   = "all",
    [UCC_CL_LAST]  = NULL
};

UCC_CLASS_INIT_FUNC(ucc_cl_lib_t, ucc_cl_iface_t *cl_iface,
                    const ucc_cl_lib_config_t *cl_config)
{
    UCC_CLASS_CALL_BASE_INIT();
    self->iface         = cl_iface;
    self->super.log_component = cl_config->super.log_component;
    ucc_strncpy_safe(self->super.log_component.name,
                     cl_iface->cl_lib_config.name,
                     sizeof(self->super.log_component.name));
    self->super.score_str = strdup(cl_config->super.score_str);
    return UCC_OK;
}

UCC_CLASS_CLEANUP_FUNC(ucc_cl_lib_t)
{
    ucc_free(self->super.score_str);
}

UCC_CLASS_DEFINE(ucc_cl_lib_t, void);

ucc_status_t ucc_parse_cls_string(const char *cls_str,
                                  ucc_cl_type_t **cls_array, int *n_cls)
{
    int            cls_selected[UCC_CL_LAST] = {0};
    int            n_cls_selected            = 0;
    char          *cls_copy, *saveptr, *cl;
    ucc_cl_type_t *cls;
    ucc_cl_type_t  cl_type;
    cls_copy = strdup(cls_str);
    if (!cls_copy) {
        ucc_error("failed to create a copy cls string");
        return UCC_ERR_NO_MEMORY;
    }
    cl = strtok_r(cls_copy, ",", &saveptr);
    while (NULL != cl) {
        cl_type = ucc_cl_name_to_type(cl);
        if (cl_type == UCC_CL_LAST) {
            ucc_error("incorrect value is passed as part of UCC_CLS list: %s",
                      cl);
            ucc_free(cls_copy);
            return UCC_ERR_INVALID_PARAM;
        }
        n_cls_selected++;
        cls_selected[cl_type] = 1;
        cl                    = strtok_r(NULL, ",", &saveptr);
    }
    ucc_free(cls_copy);
    if (n_cls_selected == 0) {
        ucc_error("incorrect value is passed as part of UCC_CLS list: %s",
                  cls_str);
        return UCC_ERR_INVALID_PARAM;
    }
    cls = ucc_malloc(n_cls_selected * sizeof(ucc_cl_type_t), "cls_array");
    if (!cls) {
        ucc_error("failed to allocate %zd bytes for cls_array",
                  n_cls_selected * sizeof(int));
        return UCC_ERR_NO_MEMORY;
    }
    n_cls_selected = 0;
    for (cl_type = 0; cl_type < UCC_CL_LAST; cl_type++) {
        if (cls_selected[cl_type]) {
            cls[n_cls_selected++] = cl_type;
        }
    }
    *cls_array = cls;
    *n_cls     = n_cls_selected;
    return UCC_OK;
}

UCC_CLASS_INIT_FUNC(ucc_cl_context_t, ucc_cl_lib_t *cl_lib,
                    ucc_context_t *ucc_context)
{
    UCC_CLASS_CALL_BASE_INIT();
    self->super.lib         = &cl_lib->super;
    self->super.ucc_context = ucc_context;
    return UCC_OK;
}

UCC_CLASS_CLEANUP_FUNC(ucc_cl_context_t)
{
}

UCC_CLASS_DEFINE(ucc_cl_context_t, void);

ucc_status_t ucc_cl_context_config_read(ucc_cl_lib_t *cl_lib,
                                        const ucc_context_config_t *config,
                                        ucc_cl_context_config_t **cl_config)
{
    ucc_status_t status;
    status = ucc_base_config_read(config->lib->full_prefix,
                                  &cl_lib->iface->cl_context_config,
                                  (ucc_base_config_t **)cl_config);
    if (UCC_OK == status) {
        (*cl_config)->cl_lib = cl_lib;
    }
    return status;
}

ucc_status_t ucc_cl_lib_config_read(ucc_cl_iface_t *iface,
                                    const char *full_prefix,
                                    ucc_cl_lib_config_t **cl_config)
{
    return ucc_base_config_read(full_prefix, &iface->cl_lib_config,
                                (ucc_base_config_t **)cl_config);
}

UCC_CLASS_INIT_FUNC(ucc_cl_team_t, ucc_cl_context_t *cl_context)
{
    UCC_CLASS_CALL_BASE_INIT();
    self->super.context = &cl_context->super;
    return UCC_OK;
}

UCC_CLASS_CLEANUP_FUNC(ucc_cl_team_t)
{
}

UCC_CLASS_DEFINE(ucc_cl_team_t, void);
