/* -*- Mode: C; c-basic-offset:4 ; -*- */
/*
 *  (C) 2008 by Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include "hydra.h"
#include "pmi_handle.h"
#include "pmi_utils.h"

struct HYD_pmcd_pmi_handle *HYD_pmcd_pmi_handle = { 0 };

HYD_status HYD_pmcd_args_to_tokens(char *args[], struct HYD_pmcd_token **tokens, int *count)
{
    int i, j;
    char *arg;
    HYD_status status = HYD_SUCCESS;

    for (i = 0; args[i]; i++);
    *count = i;
    HYDU_MALLOC(*tokens, struct HYD_pmcd_token *, *count * sizeof(struct HYD_pmcd_token),
                status);

    for (i = 0; args[i]; i++) {
        arg = HYDU_strdup(args[i]);
        (*tokens)[i].key = arg;
        for (j = 0; arg[j] && arg[j] != '='; j++);
        if (!arg[j]) {
            (*tokens)[i].val = NULL;
        }
        else {
            arg[j] = 0;
            (*tokens)[i].val = &arg[++j];
        }
    }

  fn_exit:
    return status;

  fn_fail:
    goto fn_exit;
}

void HYD_pmcd_free_tokens(struct HYD_pmcd_token *tokens, int token_count)
{
    int i;

    for (i = 0; i < token_count; i++)
        HYDU_FREE(tokens[i].key);
    HYDU_FREE(tokens);
}

HYD_status HYD_pmcd_segment_tokens(struct HYD_pmcd_token *tokens, int token_count,
                                   struct HYD_pmcd_token_segment *segment_list)
{
    int i, j;
    HYD_status status = HYD_SUCCESS;

    j = 0;
    segment_list[j].start_idx = 0;
    segment_list[j].token_count = 0;
    for (i = 0; i < token_count; i++) {
        if (!strcmp(tokens[i].key, "endcmd") && (i < token_count - 1)) {
            j++;
            segment_list[j].start_idx = i + 1;
            segment_list[j].token_count = 0;
        }
        else {
            segment_list[j].token_count++;
        }
    }

  fn_exit:
    return status;

  fn_fail:
    goto fn_exit;
}

char *HYD_pmcd_find_token_keyval(struct HYD_pmcd_token *tokens, int count, const char *key)
{
    int i;

    for (i = 0; i < count; i++) {
        if (!strcmp(tokens[i].key, key))
            return tokens[i].val;
    }

    return NULL;
}

static HYD_status free_pmi_kvs_list(struct HYD_pmcd_pmi_kvs *kvs_list)
{
    struct HYD_pmcd_pmi_kvs_pair *key_pair, *tmp;
    HYD_status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    key_pair = kvs_list->key_pair;
    while (key_pair) {
        tmp = key_pair->next;
        HYDU_FREE(key_pair);
        key_pair = tmp;
    }
    HYDU_FREE(kvs_list);

    HYDU_FUNC_EXIT();
    return status;
}

HYD_status HYD_pmcd_pmi_add_kvs(const char *key, char *val, struct HYD_pmcd_pmi_kvs * kvs,
                                int *ret)
{
    struct HYD_pmcd_pmi_kvs_pair *key_pair, *run;
    HYD_status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    HYDU_MALLOC(key_pair, struct HYD_pmcd_pmi_kvs_pair *, sizeof(struct HYD_pmcd_pmi_kvs_pair),
                status);
    HYDU_snprintf(key_pair->key, MAXKEYLEN, "%s", key);
    HYDU_snprintf(key_pair->val, MAXVALLEN, "%s", val);
    key_pair->next = NULL;

    *ret = 0;

    if (kvs->key_pair == NULL) {
        kvs->key_pair = key_pair;
    }
    else {
        run = kvs->key_pair;
        while (run->next) {
            if (!strcmp(run->key, key_pair->key)) {
                /* duplicate key found */
                *ret = -1;
                break;
            }
            run = run->next;
        }
        run->next = key_pair;
    }

  fn_exit:
    HYDU_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}


HYD_status HYD_pmcd_pmi_id_to_rank(int id, int pgid, int *rank)
{
    struct HYD_pg *pg;
    struct HYD_pmcd_pmi_pg_scratch *pg_scratch;
    HYD_status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    if (HYD_handle.ranks_per_proc == -1) {
        /* If multiple procs per rank is not defined, use ID as the rank */
        *rank = id;
    }
    else {
        for (pg = &HYD_handle.pg_list; pg->pgid != pgid; pg = pg->next);
        if (!pg)
            HYDU_ERR_SETANDJUMP1(status, HYD_INTERNAL_ERROR, "PMI pgid %d not found\n", pgid);

        pg_scratch = (struct HYD_pmcd_pmi_pg_scratch *) pg->pg_scratch;

        *rank = (id * HYD_handle.ranks_per_proc) + pg_scratch->conn_procs[id];
        pg_scratch->conn_procs[id]++;
    }

  fn_exit:
    HYDU_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}


HYD_status HYD_pmcd_pmi_process_mapping(struct HYD_pmcd_pmi_process *process,
                                        char **process_mapping_str)
{
    int i, node_id;
    char *tmp[HYD_NUM_TMP_STRINGS];
    struct HYD_proxy *proxy;
    struct block {
        int num_blocks;
        int block_size;
        struct block *next;
    } *blocklist_head, *blocklist_tail = NULL, *block, *nblock;
    HYD_status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    blocklist_head = NULL;
    for (proxy = HYD_handle.pg_list.proxy_list; proxy; proxy = proxy->next) {
        if (blocklist_head == NULL) {
            HYDU_MALLOC(blocklist_head, struct block *, sizeof(struct block), status);
            blocklist_head->block_size = proxy->node.core_count;
            blocklist_head->num_blocks = 1;
            blocklist_head->next = NULL;
            blocklist_tail = blocklist_head;
        }
        else if (blocklist_tail->block_size == proxy->node.core_count) {
            blocklist_tail->num_blocks++;
        }
        else {
            HYDU_MALLOC(blocklist_tail->next, struct block *, sizeof(struct block), status);
            blocklist_tail = blocklist_tail->next;
            blocklist_tail->block_size = proxy->node.core_count;
            blocklist_tail->num_blocks = 1;
            blocklist_tail->next = NULL;
        }
    }

    i = 0;
    tmp[i++] = HYDU_strdup("(");
    tmp[i++] = HYDU_strdup("vector,");
    node_id = 0;
    for (block = blocklist_head; block; block = block->next) {
        tmp[i++] = HYDU_strdup("(");
        tmp[i++] = HYDU_int_to_str(node_id++);
        tmp[i++] = HYDU_strdup(",");
        tmp[i++] = HYDU_int_to_str(block->num_blocks);
        tmp[i++] = HYDU_strdup(",");
        tmp[i++] = HYDU_int_to_str(block->block_size);
        tmp[i++] = HYDU_strdup(")");
        if (block->next)
            tmp[i++] = HYDU_strdup(",");
        HYDU_STRLIST_CONSOLIDATE(tmp, i, status);
    }
    tmp[i++] = HYDU_strdup(")");
    tmp[i++] = NULL;

    status = HYDU_str_alloc_and_join(tmp, process_mapping_str);
    HYDU_ERR_POP(status, "error while joining strings\n");

    HYDU_free_strlist(tmp);

    for (block = blocklist_head; block; block = nblock) {
        nblock = block->next;
        HYDU_FREE(block);
    }

  fn_exit:
    HYDU_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}

HYD_status HYD_pmcd_pmi_add_process_to_pg(struct HYD_pg *pg, int fd, int rank)
{
    struct HYD_pmcd_pmi_process *process, *tmp;
    struct HYD_proxy *proxy;
    struct HYD_pmcd_pmi_proxy_scratch *proxy_scratch;
    int srank;
    HYD_status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    srank = rank % HYD_handle.global_core_count;

    for (proxy = pg->proxy_list; proxy; proxy = proxy->next)
        if ((srank >= proxy->start_pid) &&
            (srank < (proxy->start_pid + proxy->node.core_count)))
            break;

    if (proxy->proxy_scratch == NULL) {
        HYDU_MALLOC(proxy->proxy_scratch, void *, sizeof(struct HYD_pmcd_pmi_proxy_scratch),
                    status);

        proxy_scratch = (struct HYD_pmcd_pmi_proxy_scratch *) proxy->proxy_scratch;
        proxy_scratch->process_list = NULL;
        proxy_scratch->kvs = NULL;
    }

    proxy_scratch = (struct HYD_pmcd_pmi_proxy_scratch *) proxy->proxy_scratch;

    if (proxy_scratch->kvs == NULL) {
        status = HYD_pmcd_pmi_allocate_kvs(&proxy_scratch->kvs, pg->pgid);
        HYDU_ERR_POP(status, "unable to allocate kvs space\n");
    }

    /* Add process to the node */
    HYDU_MALLOC(process, struct HYD_pmcd_pmi_process *, sizeof(struct HYD_pmcd_pmi_process),
                status);
    process->fd = fd;
    process->rank = rank;
    process->epoch = 0;
    process->proxy = proxy;
    process->next = NULL;

    if (proxy_scratch->process_list == NULL)
        proxy_scratch->process_list = process;
    else {
        tmp = proxy_scratch->process_list;
        while (tmp->next)
            tmp = tmp->next;
        tmp->next = process;
    }

  fn_exit:
    HYDU_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}

struct HYD_pmcd_pmi_process *HYD_pmcd_pmi_find_process(int fd)
{
    struct HYD_pg *pg;
    struct HYD_proxy *proxy;
    struct HYD_pmcd_pmi_proxy_scratch *proxy_scratch;
    struct HYD_pmcd_pmi_process *process = NULL;

    for (pg = &HYD_handle.pg_list; pg; pg = pg->next) {
        for (proxy = pg->proxy_list; proxy; proxy = proxy->next) {
            if (proxy->proxy_scratch == NULL)
                continue;

            proxy_scratch = (struct HYD_pmcd_pmi_proxy_scratch *) proxy->proxy_scratch;
            for (process = proxy_scratch->process_list; process; process = process->next) {
                if (process->fd == fd)
                    return process;
            }
        }
    }

    return NULL;
}

HYD_status HYD_pmcd_pmi_finalize(void)
{
    struct HYD_pg *pg;
    struct HYD_proxy *proxy;
    struct HYD_pmcd_pmi_pg_scratch *pg_scratch;
    struct HYD_pmcd_pmi_proxy_scratch *proxy_scratch;
    struct HYD_pmcd_pmi_process *process, *tmp;
    HYD_status status = HYD_SUCCESS;

    HYDU_FUNC_ENTER();

    for (pg = &HYD_handle.pg_list; pg; pg = pg->next) {
        pg_scratch = (struct HYD_pmcd_pmi_pg_scratch *) pg->pg_scratch;

        if (pg_scratch->conn_procs)
            HYDU_FREE(pg_scratch->conn_procs);

        status = free_pmi_kvs_list(pg_scratch->kvs);
        HYDU_ERR_POP(status, "unable to free kvs list\n");

        HYDU_FREE(pg_scratch);
        pg->pg_scratch = NULL;

        for (proxy = pg->proxy_list; proxy; proxy = proxy->next) {
            proxy_scratch = (struct HYD_pmcd_pmi_proxy_scratch *) proxy->proxy_scratch;

            for (process = proxy_scratch->process_list; process;) {
                tmp = process->next;
                HYDU_FREE(process);
                process = tmp;
            }

            status = free_pmi_kvs_list(proxy_scratch->kvs);
            HYDU_ERR_POP(status, "unable to free kvs list\n");

            HYDU_FREE(proxy_scratch);
        }
    }

  fn_exit:
    HYDU_FUNC_EXIT();
    return status;

  fn_fail:
    goto fn_exit;
}
