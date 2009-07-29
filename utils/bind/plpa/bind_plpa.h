/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2008 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#ifndef BIND_PLPA_H_INCLUDED
#define BIND_PLPA_H_INCLUDED

HYD_Status HYDU_bind_plpa_init(char *user_bind_map, HYDU_bind_support_level_t *support_level);
HYD_Status HYDU_bind_plpa_process(int core);

#endif /* BIND_PLPA_H_INCLUDED */
