/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2008 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "hydra.h"
#include "hydra_utils.h"
#include "bsci.h"
#include "bscu.h"
#include "slurm.h"

extern HYD_Handle handle;

HYD_Status HYD_BSCD_slurm_launch_procs(void)
{
    struct HYD_Partition *partition;
    char *client_arg[HYD_NUM_TMP_STRINGS];
    char *tmp[HYD_NUM_TMP_STRINGS], *path = NULL, *test_path = NULL;
    int i, arg, process_id;
    HYD_Status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    /*
     * We use the following priority order for the executable path:
     *    1. User-specified: handle.bootstrap_exec
     *    2. Search in path
     *    3. Hard-coded location
     */
    if (handle.bootstrap_exec) {
        path = HYDU_strdup(handle.bootstrap_exec);
    }
    else {
        status = HYDU_find_in_path("srun", &test_path);
        HYDU_ERR_POP(status, "error while searching for executable in user path\n");

        if (test_path) {
            tmp[0] = test_path;
            tmp[1] = "srun";
            tmp[2] = NULL;

            status = HYDU_str_alloc_and_join(tmp, &path);
            HYDU_ERR_POP(status, "error joining strings\n");
        }
        else
            path = HYDU_strdup("/usr/bin/srun");
    }

    /* FIXME: Instead of directly reading from the HYD_Handle
     * structure, the upper layers should be able to pass what exactly
     * they want launched. Without this functionality, the proxy
     * cannot use this and will have to perfom its own launch. */
    process_id = 0;
    FORALL_ACTIVE_PARTITIONS(partition, handle.partition_list) {
        /* Setup the executable arguments */
        arg = 0;

        /* FIXME: Get the path to srun */
        if (handle.bootstrap_exec)
            client_arg[arg++] = HYDU_strdup(handle.bootstrap_exec);
        else
            client_arg[arg++] = HYDU_strdup("srun");

        /* Currently, we do not support any partition names other than
         * host names */
        client_arg[arg++] = HYDU_strdup(partition->base->name);

        for (i = 0; partition->base->proxy_args[i]; i++)
            client_arg[arg++] = HYDU_strdup(partition->base->proxy_args[i]);

        client_arg[arg++] = NULL;

        if (HYD_BSCI_debug) {
            HYDU_Dump("Launching process: ");
            HYDU_print_strlist(client_arg);
        }

        /* The stdin pointer will be some value for process_id 0; for
         * everyone else, it's NULL. */
        status = HYDU_create_process(client_arg, NULL,
                                     (process_id == 0 ? &partition->base->in : NULL),
                                     &partition->base->out, &partition->base->err,
                                     &partition->base->pid, -1);
        if (status != HYD_SUCCESS) {
            HYDU_Error_printf("bootstrap spawn process returned error\n");
            goto fn_fail;
        }

        for (arg = 0; client_arg[arg]; arg++)
            HYDU_FREE(client_arg[arg]);

        process_id++;
    }

  fn_exit:
    HYDU_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}
