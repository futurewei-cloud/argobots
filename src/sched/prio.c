/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/* Priority Scheduler Implementation */

static int  sched_init(ABT_sched sched, ABT_sched_config config);
static void sched_run(ABT_sched sched);
static int  sched_free(ABT_sched);

static ABT_sched_def sched_prio_def = {
    .type = ABT_SCHED_TYPE_TASK,
    .init = sched_init,
    .run = sched_run,
    .free = sched_free,
    .get_migr_pool = NULL
};

typedef struct {
    uint32_t event_freq;
#ifdef ABT_CONFIG_USE_SCHED_SLEEP
    struct timespec sleep_time;
#endif
} sched_data;


ABT_sched_def *ABTI_sched_get_prio_def(void)
{
    return &sched_prio_def;
}

static inline sched_data *sched_data_get_ptr(void *data)
{
    return (sched_data *)data;
}

static int sched_init(ABT_sched sched, ABT_sched_config config)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    /* Default settings */
    sched_data *p_data = (sched_data *)ABTU_malloc(sizeof(sched_data));
    p_data->event_freq = ABTI_global_get_sched_event_freq();
#ifdef ABT_CONFIG_USE_SCHED_SLEEP
    p_data->sleep_time.tv_sec = 0;
    p_data->sleep_time.tv_nsec = ABTI_global_get_sched_sleep_nsec();
#endif

    /* Set the variables from the config */
    void *p_event_freq = &p_data->event_freq;
    ABTI_sched_config_read(config, 1, 1, &p_event_freq);

    p_sched->data = p_data;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("prio: sched_init", abt_errno);
    goto fn_exit;
}

static void sched_run(ABT_sched sched)
{
    ABTI_local *p_local = ABTI_local_get_local();
    uint32_t work_count = 0;
    sched_data *p_data;
    uint32_t event_freq;
    int num_pools;
    ABT_pool *p_pools;
    int i;
    CNT_DECL(run_cnt);

    ABTI_xstream *p_xstream = p_local->p_xstream;
    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_ASSERT(p_sched);

    p_data = sched_data_get_ptr(p_sched->data);
    event_freq = p_data->event_freq;

    /* Get the list of pools */
    num_pools = p_sched->num_pools;
    p_pools = (ABT_pool *)ABTU_malloc(num_pools * sizeof(ABT_pool));
    memcpy(p_pools, p_sched->pools, sizeof(ABT_pool) * num_pools);

    while (1) {
        CNT_INIT(run_cnt, 0);

        /* Execute one work unit from the scheduler's pool */
        /* The pool with lower index has higher priority. */
        for (i = 0; i < num_pools; i++) {
            ABT_pool pool = p_pools[i];
            ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
            ABT_unit unit = ABTI_pool_pop(p_pool);
            if (unit != ABT_UNIT_NULL) {
                ABTI_xstream_run_unit(&p_local, p_xstream, unit, p_pool);
                CNT_INC(run_cnt);
                break;
            }
        }

        if (++work_count >= event_freq) {
            ABT_bool stop = ABTI_sched_has_to_stop(&p_local, p_sched,
                                                   p_xstream);
            if (stop == ABT_TRUE) break;
            work_count = 0;
            ABTI_xstream_check_events(p_xstream, sched);
            SCHED_SLEEP(run_cnt, p_data->sleep_time);
        }
    }

    ABTU_free(p_pools);
}

static int sched_free(ABT_sched sched)
{
    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_ASSERT(p_sched);

    sched_data *p_data = sched_data_get_ptr(p_sched->data);
    ABTU_free(p_data);

    return ABT_SUCCESS;
}

