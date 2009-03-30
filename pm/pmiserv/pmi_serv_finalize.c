/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2008 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "hydra.h"
#include "pmci.h"
#include "pmi_handle.h"
#include "bsci.h"
#include "demux.h"
#include "pmi_serv.h"

int HYD_PMCD_pmi_serv_listenfd;

HYD_Status HYD_PMCI_finalize(void)
{
    HYD_Status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    /* Deregister the listen socket from the demux engine and close
     * it. */
    status = HYD_DMX_deregister_fd(HYD_PMCD_pmi_serv_listenfd);
    HYDU_ERR_POP(status, "unable to deregister fd\n");

    close(HYD_PMCD_pmi_serv_listenfd);
    HYD_PMCD_pmi_serv_listenfd = -1;

    status = HYD_PMCD_pmi_finalize();
    HYDU_ERR_POP(status, "unable to finalize process manager utils\n");

    if (handle.launch_mode == HYD_LAUNCH_RUNTIME) {
        status = HYD_BSCI_finalize();
        HYDU_ERR_POP(status, "unable to finalize bootstrap server\n");
    }

    status = HYD_DMX_finalize();
    HYDU_ERR_POP(status, "error returned from demux finalize\n");

  fn_exit:
    HYDU_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}
