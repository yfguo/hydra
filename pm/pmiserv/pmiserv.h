/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2008 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef PMISERV_H_INCLUDED
#define PMISERV_H_INCLUDED

#include "pmi_common.h"

HYD_status HYD_pmcd_pmiserv_proxy_init_cb(int fd, HYD_event_t events, void *userp);
HYD_status HYD_pmcd_pmiserv_control_listen_cb(int fd, HYD_event_t events, void *userp);
HYD_status HYD_pmcd_pmiserv_cleanup(void);

#endif /* PMISERV_H_INCLUDED */
