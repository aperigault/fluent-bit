/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2015-2018 Treasure Data Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef FLB_TASK_H
#define FLB_TASK_H

#include <monkey/mk_core.h>
#include <fluent-bit/flb_info.h>
#include <fluent-bit/flb_buffer.h>
#include <fluent-bit/flb_input.h>

/* Task status */
#define FLB_TASK_NEW      0
#define FLB_TASK_RUNNING  1

/*
 * Macro helpers to determinate return value, task_id and thread_id. When an
 * output plugin returns, it must call FLB_OUTPUT_RETURN(val) where val is
 * the return value, as of now defined as FLB_OK or FLB_ERROR.
 *
 * The FLB_OUTPUT_RETURN macro lookup the current active 'engine thread' and
 * it 'engine task' associated, so it emits an event to the main event loop
 * indicating an output thread has done. In order to specify return values
 * and the proper IDs an unsigned 32 bits number is used:
 *
 *     AAAA     BBBBBBBBBBBBBB CCCCCCCCCCCCCC   > 32 bit number
 *       ^            ^              ^
 *    4 bits       14 bits        14 bits
 *  return val     task_id       thread_id
 */

#define FLB_TASK_RET(val)  (val >> 28)
#define FLB_TASK_ID(val)   (uint32_t) (val & 0xfffc000) >> 14
#define FLB_TASK_TH(val)   (val & 0x3fff)
#define FLB_TASK_SET(ret, task_id, th_id)               \
    (uint32_t) ((ret << 28) | (task_id << 14) | th_id)

struct flb_task_route {
    struct flb_output_instance *out;
    struct mk_list _head;
};

/*
 * When a Task failed in an output instance plugin and this last one
 * requested a FLB_RETRY, a flb_engine_task_retry entry is created and
 * linked into the parent flb_engine_task->retries lists.
 *
 * This reference is used later by the scheduler to re-dispatch the
 * task data to the desired output path.
 */
struct flb_task_retry {
    int attemps;                        /* number of attemps, default 1 */
    struct flb_output_instance *o_ins;  /* route that we are retrying   */
    struct flb_task *parent;            /* parent task reference        */
    struct mk_list _head;               /* link to parent task list     */
};

/* A task takes a buffer and sync input and output instances to handle it */
struct flb_task {
    int id;                             /* task id                   */
    uint64_t ref_id;                    /* external reference id     */
    int status;                         /* new task or running ?     */
    int mapped;                         /* is a mmap(2)ed file ?     */
    int deleted;                        /* should be deleted ?       */
    int n_threads;                      /* number number of threads  */
    int users;                          /* number of users (threads) */
    int destinations;                   /* number of output dests    */
    char *tag;                          /* original tag              */
    char *buf;                          /* buffer                    */
    size_t size;                        /* buffer data size          */
#ifdef FLB_HAVE_BUFFERING
    int worker_id;                      /* Buffer worker that owns this task */
    int qchunk_id;                      /* qchunk id if it comes from buffer */
    unsigned char hash_sha1[20];        /* SHA1(buf)                         */
    char hash_hex[41];                  /* Hex string for hash_sha1          */
#endif
    struct flb_input_dyntag *dt;        /* dyntag node (if applies)      */
    struct flb_input_instance *i_ins;   /* input instance                */
    struct mk_list threads;             /* ref flb_input_instance->tasks */
    struct mk_list routes;              /* routes to dispatch data       */
    struct mk_list retries;             /* queued in-memory retries      */
    struct mk_list _head;               /* link to input_instance        */
    struct flb_config *config;          /* parent flb config             */

#ifdef FLB_HAVE_FLUSH_PTHREADS
    pthread_mutex_t mutex_threads;
#endif
};

struct flb_task *flb_task_create(uint64_t ref_id,
                                 char *buf,
                                 size_t size,
                                 struct flb_input_instance *i_ins,
                                 struct flb_input_dyntag *dt,
                                 char *tag,
                                 struct flb_config *config);
struct flb_task *flb_task_create_direct(uint64_t ref_id,
                                        char *buf,
                                        size_t size,
                                        struct flb_input_instance *i_ins,
                                        char *tag,
                                        char *hash,
                                        uint64_t routes,
                                        struct flb_config *config);

void flb_task_destroy(struct flb_task *task);

struct flb_task_retry *flb_task_retry_create(struct flb_task *task,
                                             void *data);
void flb_task_retry_destroy(struct flb_task_retry *retry);
int flb_task_retry_clean(struct flb_task *task, void *data);

#endif
