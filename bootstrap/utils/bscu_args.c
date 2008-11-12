/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2008 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "hydra.h"
#include "hydra_dbg.h"
#include "hydra_mem.h"
#include "csi.h"
#include "bsci.h"
#include "bscu.h"

HYD_CSI_Handle csi_handle;

static int use_csh_format(void)
{
    int format = 0;
    char * shell, * sname;

    /* format = 1 represents csh; format = 0 represents sh */
    shell = getenv("SHELL");
    sname = strrchr(shell, '/');
    if (!sname)
        sname = shell;
    else
        sname++;
    if (shell) {
        if (!strcmp(sname, "bash") || !strcmp(sname, "sh") ||
            !strcmp(sname, "ash"))
            format = 0;
        else
            format = 1;
    }
    else {
        format = 1;
    }

    return format;
}

#if defined FUNCNAME
#undef FUNCNAME
#endif /* FUNCNAME */
#define FUNCNAME "HYD_BSCU_Append_env"
HYD_Status HYD_BSCU_Append_env(HYDU_Env_t * env_list, char **client_arg, int id)
{
    int i, j, csh_format;
    HYDU_Env_t *env;
    char *envstr, *tmp[HYDU_NUM_JOIN_STR], *inc;
    HYD_Status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    csh_format = use_csh_format();

    for (i = 0; client_arg[i]; i++);
    env = env_list;
    while (env) {
	j = 0;

        if (csh_format) {
            tmp[j++] = MPIU_Strdup("setenv");
            tmp[j++] = MPIU_Strdup(env->env_name);
        }
        else {
            tmp[j++] = MPIU_Strdup(env->env_name);
            tmp[j++] = MPIU_Strdup("=");
        }

	if (env->env_type == HYDU_ENV_STATIC)
	    tmp[j++] = MPIU_Strdup(env->env_value);
	else if (env->env_type == HYDU_ENV_AUTOINC) {
	    HYDU_Int_to_str(env->start_val + id, inc, status);
	    tmp[j++] = MPIU_Strdup(inc);
	    HYDU_FREE(inc);
	}

	tmp[j++] = NULL;
	HYDU_STR_ALLOC_AND_JOIN(tmp, envstr, status);
	client_arg[i++] = MPIU_Strdup(envstr);
	HYDU_FREE(envstr);
	for (j = 0; tmp[j]; j++)
	    HYDU_FREE(tmp[j]);

	client_arg[i++] = MPIU_Strdup(";");

        if (csh_format == 0) {
            client_arg[i++] = MPIU_Strdup("export");
            client_arg[i++] = MPIU_Strdup(env->env_name);
            client_arg[i++] = MPIU_Strdup(";");
        }

	env = env->next;
    }
    client_arg[i++] = NULL;

  fn_exit:
    HYDU_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}


#if defined FUNCNAME
#undef FUNCNAME
#endif /* FUNCNAME */
#define FUNCNAME "HYD_BSCU_Append_exec"
HYD_Status HYD_BSCU_Append_exec(char **exec, char **client_arg)
{
    int i, j;
    HYD_Status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    for (i = 0; client_arg[i]; i++);
    for (j = 0; exec[j]; j++)
	client_arg[i++] = MPIU_Strdup(exec[j]);
    client_arg[i++] = NULL;

  fn_exit:
    HYDU_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}


#if defined FUNCNAME
#undef FUNCNAME
#endif /* FUNCNAME */
#define FUNCNAME "HYD_BSCU_Append_wdir"
HYD_Status HYD_BSCU_Append_wdir(char **client_arg)
{
    int arg;
    HYD_Status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    for (arg = 0; client_arg[arg]; arg++);
    client_arg[arg++] = MPIU_Strdup("cd");
    client_arg[arg++] = MPIU_Strdup(csi_handle.wdir);
    client_arg[arg++] = MPIU_Strdup(";");
    client_arg[arg++] = NULL;

  fn_exit:
    HYDU_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}
