/**
* Copyright (C) Mellanox Technologies Ltd. 2001-2015.  ALL RIGHTS RESERVED.
*
* See file LICENSE for terms.
*/

#ifndef UCX_INFO_H
#define UCX_INFO_H

#include <uct/api/uct.h>
#include <ucp/api/ucp.h>


enum {
    PRINT_VERSION        = UCS_BIT(0),
    PRINT_SYS_INFO       = UCS_BIT(1),
    PRINT_BUILD_CONFIG   = UCS_BIT(2),
    PRINT_TYPES          = UCS_BIT(3),
    PRINT_DEVICES        = UCS_BIT(4),
    PRINT_PROTOCOLS      = UCS_BIT(5)
};


void print_ucp_config(ucs_config_print_flags_t print_flags);

void print_uct_config(ucs_config_print_flags_t print_flags, const char *tl_name);

void print_version();

void print_sys_info();

void print_build_config();

void print_uct_info(int print_opts, ucs_config_print_flags_t print_flags,
                    const char *req_tl_name);

void print_type_info(const char * tl_name);

void print_proto_info(ucs_config_print_flags_t print_flags);

/**
 * @ingroup RESOURCE
 * @brief Print MD component configuration to a stream.
 *
 * @param [in]  print_flags   Controls how the configuration is printed.
 */
void uct_md_component_config_print(ucs_config_print_flags_t print_flags);

#endif
