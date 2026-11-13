/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/
#include "unitcheck.h"

#include "curlx/wait.h"
#include "thrdqueue.h"
#include "curl_threads.h"

#ifdef USE_THREADS

static curl_malloc_callback unit3312_malloc;
static curl_calloc_callback unit3312_calloc;
static curl_free_callback unit3312_free;
static curl_mutex_t unit3312_lock;
static curl_thread_id_t unit3312_creator;
static int unit3312_fail_at; /* the allocation to fail, counted from 1 */
static int unit3312_nallocs;
static int unit3312_live;

static bool unit3312_fail_next(void)
{
  bool fail;
  Curl_mutex_acquire(&unit3312_lock);
  fail = (++unit3312_nallocs == unit3312_fail_at);
  Curl_mutex_release(&unit3312_lock);
  return fail;
}

static void *unit3312_allocated(void *ptr)
{
  if(ptr) {
    Curl_mutex_acquire(&unit3312_lock);
    ++unit3312_live;
    Curl_mutex_release(&unit3312_lock);
  }
  return ptr;
}

static int unit3312_live_count(void)
{
  int live;
  Curl_mutex_acquire(&unit3312_lock);
  live = unit3312_live;
  Curl_mutex_release(&unit3312_lock);
  return live;
}

static void *unit3312_malloc_cb(size_t size)
{
  if(unit3312_fail_next())
    return NULL;
  return unit3312_allocated(unit3312_malloc(size));
}

static void *unit3312_calloc_cb(size_t nmemb, size_t size)
{
  if(unit3312_fail_next())
    return NULL;
  return unit3312_allocated(unit3312_calloc(nmemb, size));
}

static void unit3312_free_cb(void *ptr)
{
  /* a worker thread frees its start arguments first. Holding it here
     keeps it alive past any create call that does not wait for it. */
  if(!Curl_thread_is_current(unit3312_creator))
    curlx_wait_ms(20);
  if(ptr) {
    Curl_mutex_acquire(&unit3312_lock);
    --unit3312_live;
    Curl_mutex_release(&unit3312_lock);
  }
  unit3312_free(ptr);
}

static void unit3312_item_free(void *item)
{
  (void)item;
}

static void unit3312_process(void *item)
{
  (void)item;
}

/* Create a queue with 2 minimum threads, failing one allocation after
 * the other until the create succeeds. This covers failures after the
 * first thread was started. A failed create must have ended all threads
 * it started and released everything it allocated when it returns. */
static CURLcode test_unit3312(const char *arg)
{
  UNITTEST_BEGIN_SIMPLE
  struct curl_thrdq *tqueue = NULL;
  CURLcode result = CURLE_OUT_OF_MEMORY;
  int i;

  Curl_mutex_init(&unit3312_lock);
  unit3312_creator = Curl_thread_get_current_id();
  unit3312_malloc = Curl_cmalloc;
  unit3312_calloc = Curl_ccalloc;
  unit3312_free = Curl_cfree;
  Curl_cmalloc = unit3312_malloc_cb;
  Curl_ccalloc = unit3312_calloc_cb;
  Curl_cfree = unit3312_free_cb;

  for(i = 1; result && (i < 100); ++i) {
    Curl_mutex_acquire(&unit3312_lock);
    unit3312_fail_at = i;
    unit3312_nallocs = 0;
    Curl_mutex_release(&unit3312_lock);

    result = Curl_thrdq_create(&tqueue, "unit3312", 2, 2, 0,
                               unit3312_item_free, unit3312_process,
                               NULL, NULL);
    if(result) {
      fail_unless(!tqueue, "failed create returned a queue");
      fail_unless(!unit3312_live_count(),
                  "failed create left threads or memory behind");
    }
  }
  fail_unless(!result, "queue create");
  fail_unless(i > 6, "no create failed with a thread started");

  Curl_mutex_acquire(&unit3312_lock);
  unit3312_fail_at = 0;
  Curl_mutex_release(&unit3312_lock);
  if(tqueue)
    Curl_thrdq_destroy(tqueue, TRUE);
  tqueue = NULL;
  fail_unless(!unit3312_live_count(), "destroy left memory behind");

  Curl_cmalloc = unit3312_malloc;
  Curl_ccalloc = unit3312_calloc;
  Curl_cfree = unit3312_free;
  Curl_mutex_destroy(&unit3312_lock);

  UNITTEST_END_SIMPLE
}

#else
static CURLcode test_unit3312(const char *arg)
{
  UNITTEST_BEGIN_SIMPLE
  UNITTEST_END_SIMPLE
}
#endif /* USE_THREADS */
