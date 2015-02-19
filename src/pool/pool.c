/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/** @defgroup POOL Pool
 * This group is for Pool.
 */

/**
 * @ingroup POOL
 * @brief   Create a new pool and return its handle through newpool.
 *
 * This function let the user create his own pool.
 *
 * @param[in]  def     definition required for pool creation
 * @param[out] newpool handle to a new pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_create(ABT_pool_def *def, ABT_pool_config config,
                    ABT_pool *newpool)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool;

    p_pool = (ABTI_pool *)ABTU_malloc(sizeof(ABTI_pool));
    if (!p_pool) {
        HANDLE_ERROR("ABTU_malloc");
        *newpool = ABT_POOL_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_pool->access               = def->access;
    p_pool->automatic            = ABT_FALSE;
    p_pool->num_scheds           = 0;
    p_pool->reader               = NULL;
    p_pool->writer               = NULL;
    p_pool->num_blocked          = 0;
    p_pool->num_migrations       = 0;
    p_pool->data                 = NULL;

    /* Set up the pool functions from def */
    p_pool->u_get_type           = def->u_get_type;
    p_pool->u_get_thread         = def->u_get_thread;
    p_pool->u_get_task           = def->u_get_task;
    p_pool->u_create_from_thread = def->u_create_from_thread;
    p_pool->u_create_from_task   = def->u_create_from_task;
    p_pool->u_free               = def->u_free;
    p_pool->p_init               = def->p_init;
    p_pool->p_get_size           = def->p_get_size;
    p_pool->p_push               = def->p_push;
    p_pool->p_pop                = def->p_pop;
    p_pool->p_remove             = def->p_remove;
    p_pool->p_free               = def->p_free;

    *newpool = ABTI_pool_get_handle(p_pool);

    /* Configure the pool */
    if (p_pool->p_init)
        p_pool->p_init(*newpool, config);


  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Create a new pool from a predefined type and return its handle
 * through newpool.
 *
 * @param[in]  kind    name of the predefined pool
 * @param[out] newpool handle to a new pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_create_basic(ABT_pool_kind kind, ABT_pool_access access,
                          ABT_pool *newpool)
{
    int abt_errno = ABT_SUCCESS;
    ABT_pool_def def;

    switch (kind) {
        case ABT_POOL_FIFO:
            abt_errno = ABTI_pool_get_fifo_def(access, &def);
            break;
        default:
            abt_errno = ABT_ERR_INV_POOL_KIND;
            break;
    }
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABT_pool_create(&def, ABT_POOL_CONFIG_NULL, newpool);
    ABTI_CHECK_ERROR(abt_errno);
    ABTI_pool *p_pool = ABTI_pool_get_ptr(*newpool);
    p_pool->automatic = ABT_TRUE;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_pool_create_basic", abt_errno);
    *newpool = ABT_SCHED_NULL;
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Free the given pool, and modify its value to ABT_POOL_NULL
 *
 * @param[inout] pool handle
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_free(ABT_pool *pool)
{
    int abt_errno = ABT_SUCCESS;

    ABT_pool h_pool = *pool;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(h_pool);

    if (p_pool == NULL || h_pool == ABT_POOL_NULL) {
        abt_errno = ABT_ERR_INV_POOL;
        goto fn_fail;
    }

    p_pool->p_free(h_pool);
    ABTU_free(p_pool);

    *pool = ABT_POOL_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_pool_free", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Get the access type of target pool
 *
 * @param[in]  pool      handle to the pool
 * @param[out] p_access  access type
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_get_access(ABT_pool pool, ABT_pool_access *p_access)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    *p_access = p_pool->access;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_pool_get_access", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Return the total size of a pool
 *
 * The returned size is the number of elements in the pool (provided by the
 * specific function in case of a user-defined pool), plus the number of
 * blocked ULTs and migrating ULTs.
 *
 * @param[in] pool handle to the pool
 * @param[out] size size of the pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_get_total_size(ABT_pool pool, size_t *p_size)
{
    int abt_errno = ABT_SUCCESS;
    size_t size;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    size = p_pool->p_get_size(pool);
    size += p_pool->num_blocked;
    size += p_pool->num_migrations;
    *p_size = size;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_pool_get_total_size", abt_errno);
    goto fn_exit;
}


/**
 * @ingroup POOL
 * @brief   Return the size of a pool
 *
 * The returned size is the number of elements in the pool (provided by the
 * specific function in case of a user-defined pool).
 *
 * @param[in] pool handle to the pool
 * @param[out] size size of the pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_get_size(ABT_pool pool, size_t *p_size)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    *p_size = p_pool->p_get_size(pool);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_pool_get_size", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Pop a unit from the target pool
 *
 * @param[in] pool handle to the pool
 * @param[out] p_unit handle to the unit
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_pop(ABT_pool pool, ABT_unit *p_unit)
{
    int abt_errno = ABT_SUCCESS;
    ABT_unit unit;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    unit = p_pool->p_pop(pool);

  fn_exit:
    *p_unit = unit;
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_pool_pop", abt_errno);
    unit = ABT_UNIT_NULL;
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Push a unit to the target pool
 *
 * @param[in] pool handle to the pool
 * @param[in] unit handle to the unit
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_push(ABT_pool pool, ABT_unit unit)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();

    if (unit == ABT_UNIT_NULL) {
        abt_errno = ABT_ERR_UNIT;
        goto fn_fail;
    }

    abt_errno = ABTI_pool_set_writer(p_pool, p_xstream);
    ABTI_CHECK_ERROR(abt_errno);
    p_pool->p_push(pool, unit);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_pool_push", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Remove a specified unit from the target pool
 *
 * @param[in] pool handle to the pool
 * @param[in] unit handle to the unit
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_remove(ABT_pool pool, ABT_unit unit)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    abt_errno = ABTI_pool_set_reader(p_pool, p_xstream);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = p_pool->p_remove(pool, unit);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_pool_remove", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Set the specific data of the target user-defined pool
 *
 * This function will be called by the user during the initialization of his
 * user-defined pool.
 *
 * @param[in] pool handle to the pool
 * @param[in] data specific data of the pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_set_data(ABT_pool pool, void *data)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    p_pool->data = data;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_pool_set_data", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Retrieve the specific data of the target user-defnied pool
 *
 * This function will be called by the user in a user-defined function of his
 * user-defnied pool.
 *
 * @param[in] pool handle to the pool
 * @param[in] data specific data of the pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_get_data(ABT_pool pool, void **p_data)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    *p_data = p_pool->data;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_pool_get_data", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Push a scheduler to a pool
 *
 * The scheduler should have been created by ABT_sched_create or
 * ABT_sched_create_basic.
 *
 * @param[in] pool handle to the pool
 * @param[in] sched handle to the sched
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_add_sched(ABT_pool pool, ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;
    
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    int p;

    switch (p_pool->access) {
        case ABT_POOL_ACCESS_PRW:
        case ABT_POOL_ACCESS_PR_PW:
        case ABT_POOL_ACCESS_PR_SW:
            /* we need to ensure that the target pool has already an
             * associated ES */
            if (p_pool->reader == NULL) {
                abt_errno = ABT_ERR_POOL;
                goto fn_fail;
            }
            /* We check that from the pool set of the scheduler we do not find
             * a pool with another associated pool, and set the right value if
             * it is okay  */
            for (p = 0; p < p_sched->num_pools; p++) {
                abt_errno = ABTI_pool_set_reader(p_sched->pools[p],
                                                 p_pool->reader);
                ABTI_CHECK_ERROR(abt_errno);
            }
            break;

        case ABT_POOL_ACCESS_SR_PW:
        case ABT_POOL_ACCESS_SR_SW:
            /* we need to ensure that the pool set of the scheduler does
             * not contain an ES private pool  */
            for (p = 0; p < p_sched->num_pools; p++) {
                ABTI_pool *p_pool = ABTI_pool_get_ptr(p_sched->pools[p]);
                if (p_pool->access == ABT_POOL_ACCESS_PRW ||
                    p_pool->access == ABT_POOL_ACCESS_PR_PW ||
                    p_pool->access == ABT_POOL_ACCESS_PR_SW) {
                    abt_errno = ABT_ERR_POOL;
                    goto fn_fail;
                }
            }
            break;

        default:
            abt_errno = ABT_ERR_INV_POOL_ACCESS;
            goto fn_fail;
    }

    abt_errno = ABTI_sched_associate(p_sched, ABTI_SCHED_IN_POOL);
    ABTI_CHECK_ERROR(abt_errno);

    if (p_sched->type == ABT_SCHED_TYPE_ULT) {
        abt_errno = ABT_thread_create(pool, p_sched->run, sched,
                                      ABT_THREAD_ATTR_NULL, &p_sched->thread);
        ABTI_CHECK_ERROR(abt_errno);
        ABTI_thread_get_ptr(p_sched->thread)->is_sched = p_sched;
    } else if (p_sched->type == ABT_SCHED_TYPE_TASK){
        abt_errno = ABT_task_create(pool, p_sched->run, sched, &p_sched->task);
        ABTI_CHECK_ERROR(abt_errno);
        ABTI_task_get_ptr(p_sched->task)->is_sched = p_sched;
    } else {
        abt_errno = ABT_ERR_SCHED;
        goto fn_fail;
    }

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_pool_add_sched", abt_errno);
    goto fn_exit;
}

/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

int ABTI_pool_print(ABTI_pool *p_pool)
{
    int abt_errno = ABT_SUCCESS;
    if (p_pool == NULL) {
        printf("NULL POOL\n");
        goto fn_exit;
    }
    ABT_pool pool = ABTI_pool_get_handle(p_pool);

    printf("== POOL (%p) ==\n", p_pool);
    printf("access mode: %d", p_pool->access);
    printf("automatic: %d", p_pool->automatic);
    printf("number of schedulers: %d", p_pool->num_scheds);
    printf("reader: %p", p_pool->reader);
    printf("writer: %p", p_pool->writer);
    printf("number of blocked units: %d", p_pool->num_blocked);
    printf("size: %lu", (unsigned long)p_pool->p_get_size(pool));

  fn_exit:
    return abt_errno;
}

ABTI_pool *ABTI_pool_get_ptr(ABT_pool pool)
{
    ABTI_pool *p_pool;
    if (pool == ABT_POOL_NULL) {
        p_pool = NULL;
    } else {
        p_pool = (ABTI_pool *)pool;
    }
    return p_pool;
}

ABT_pool ABTI_pool_get_handle(ABTI_pool *p_pool)
{
    ABT_pool h_pool;
    if (p_pool == NULL) {
        h_pool = ABT_POOL_NULL;
    } else {
        h_pool = (ABT_pool)p_pool;
    }
    return h_pool;
}


/* Increase num_scheds to mark the pool as having another scheduler. If the
 * pool is not available, it returns ABT_ERR_INV_POOL_ACCESS.  */
int ABTI_pool_retain(ABTI_pool *p_pool)
{
    ABTD_atomic_fetch_add_int32(&p_pool->num_scheds, 1);

    return ABT_SUCCESS;
}

/* Decrease the num_scheds to realease this pool from a scheduler. Call when
 * the pool is removed from a scheduler or when it stops. */
int ABTI_pool_release(ABTI_pool *p_pool)
{
    int abt_errno = ABT_SUCCESS;
    if (p_pool->num_scheds <= 0) {
        abt_errno = ABT_ERR_INV_POOL;
        ABTI_CHECK_ERROR(abt_errno);
    }

    ABTD_atomic_fetch_sub_int32(&p_pool->num_scheds, 1);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_pool_release", abt_errno);
    goto fn_exit;
}

/* Set the associated reader ES of a pool. This function has no effect on pools
 * of shared-read access mode.
 * If a pool is private-read to an ES, we check that the previous value of the
 * field "p_xstream" is the same as the argument of the function "p_xstream"
 * */
int ABTI_pool_set_reader(ABTI_pool *p_pool, ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    switch (p_pool->access) {
        case ABT_POOL_ACCESS_PRW:
            if (p_pool->writer && p_xstream != p_pool->writer) {
                abt_errno = ABT_ERR_INV_POOL_ACCESS;
                ABTI_CHECK_ERROR(abt_errno);
            }
        case ABT_POOL_ACCESS_PR_PW:
        case ABT_POOL_ACCESS_PR_SW:
            if (p_pool->reader && p_pool->reader != p_xstream) {
                abt_errno = ABT_ERR_INV_POOL_ACCESS;
                ABTI_CHECK_ERROR(abt_errno);
            }
            /* NB: as we do not want to use a mutex, the function can be wrong
             * here */
            p_pool->reader = p_xstream;
            break;

        case ABT_POOL_ACCESS_SR_PW:
        case ABT_POOL_ACCESS_SR_SW:
            p_pool->reader = p_xstream;
            break;

        default:
            abt_errno = ABT_ERR_INV_POOL_ACCESS;
            ABTI_CHECK_ERROR(abt_errno);
    }

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_pool_set_reader", abt_errno);
    goto fn_exit;
}

/* Set the associated writer ES of a pool. This function has no effect on pools
 * of shared-write access mode.
 * If a pool is private-write to an ES, we check that the previous value of the
 * field "p_xstream" is the same as the argument of the function "p_xstream"
 * */
int ABTI_pool_set_writer(ABTI_pool *p_pool, ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    switch (p_pool->access) {
        case ABT_POOL_ACCESS_PRW:
            if (p_pool->reader && p_xstream != p_pool->reader) {
                abt_errno = ABT_ERR_INV_POOL_ACCESS;
                goto fn_fail;
            }
        case ABT_POOL_ACCESS_PR_PW:
        case ABT_POOL_ACCESS_SR_PW:
            if (p_pool->writer && p_pool->writer != p_xstream) {
                abt_errno = ABT_ERR_INV_POOL_ACCESS;
                goto fn_fail;
            }
            /* NB: as we do not want to use a mutex, the function can be wrong
             * here */
            p_pool->writer = p_xstream;
            break;

        case ABT_POOL_ACCESS_PR_SW:
        case ABT_POOL_ACCESS_SR_SW:
            p_pool->writer = p_xstream;
            break;

        default:
            abt_errno = ABT_ERR_INV_POOL_ACCESS;
            ABTI_CHECK_ERROR(abt_errno);
    }

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_pool_set_writer", abt_errno);
    goto fn_exit;
}

/* A ULT is blocked and is waiting for going back to this pool */
int ABTI_pool_inc_num_blocked(ABTI_pool *p_pool)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_atomic_fetch_add_uint32(&p_pool->num_blocked, 1);
    return abt_errno;
}

/* A blocked ULT is back in the pool */
int ABTI_pool_dec_num_blocked(ABTI_pool *p_pool)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_atomic_fetch_sub_uint32(&p_pool->num_blocked, 1);
    return abt_errno;
}

/* The pool will receive a migrated ULT */
int ABTI_pool_inc_num_migrations(ABTI_pool *p_pool)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_atomic_fetch_add_int32(&p_pool->num_migrations, 1);
    return abt_errno;
}

/* The pool has received a migrated ULT */
int ABTI_pool_dec_num_migrations(ABTI_pool *p_pool)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_atomic_fetch_sub_int32(&p_pool->num_migrations, 1);
    return abt_errno;
}


/* Check if a pool accept migrations or not. When the writer of the destination
 * pool is ES private, we have to ensure thaht we are on the right ES */
int ABTI_pool_accept_migration(ABTI_pool *p_pool, ABTI_pool *source)
{
    switch (p_pool->access)
    {
        /* Need writer in the same ES */
        case ABT_POOL_ACCESS_PRW:
        case ABT_POOL_ACCESS_PR_PW:
        case ABT_POOL_ACCESS_SR_PW:
            if (p_pool->reader == source->writer)
                return ABT_TRUE;
            return ABT_FALSE;

        case ABT_POOL_ACCESS_PR_SW:
        case ABT_POOL_ACCESS_SR_SW:
            return ABT_TRUE;

        default:
            return ABT_FALSE;
    }
}