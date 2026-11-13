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
#include "first.h"

/*
 * A DoH probe that cannot be added to the multi handle must fail the
 * transfer right away. The add is made to fail by refusing the allocation
 * that grows the multi's transfer table.
 *
 * Do not use directly memory related functions in this file except those used
 * inside custom memory callbacks which should be calling 'the real thing'.
 */

#define T2507_MAX_FILL 4096

static CURL *t2507_fill[T2507_MAX_FILL];
static size_t t2507_rows;      /* largest pointer array allocated */
static size_t t2507_fail_rows; /* pointer array size to refuse once */

static void *t2507_calloc(size_t nmemb, size_t size)
{
  if(size == sizeof(void *)) {
    if(t2507_fail_rows && nmemb == t2507_fail_rows) {
      t2507_fail_rows = 0;
      return NULL;
    }
    if(nmemb > t2507_rows)
      t2507_rows = nmemb;
  }
  /* !checksrc! disable BANNEDFUNC 1 */
  return calloc(nmemb, size);
}

static void *t2507_malloc(size_t size)
{
  /* !checksrc! disable BANNEDFUNC 1 */
  return malloc(size);
}

static char *t2507_strdup(const char *ptr)
{
  return CURLX_STRDUP_LOW(ptr);
}

static void *t2507_realloc(void *ptr, size_t size)
{
  /* !checksrc! disable BANNEDFUNC 1 */
  return realloc(ptr, size);
}

static void t2507_free(void *ptr)
{
  /* !checksrc! disable BANNEDFUNC 1 */
  free(ptr);
}

static CURLcode test_lib2507(const char *URL)
{
  CURL *curl = NULL;
  CURLM *multi = NULL;
  CURLMsg *msg;
  CURLcode xfer_result = CURLE_OK;
  CURLcode result;
  size_t initial_rows;
  size_t grow_rows = 0;
  int still_running;
  int grow_at = 0; /* the add, counted from 1, that grows the table */
  int i;

  start_test_timing();

  result = curl_global_init_mem(CURL_GLOBAL_ALL,
                                t2507_malloc,
                                t2507_free,
                                t2507_realloc,
                                t2507_strdup,
                                t2507_calloc);
  if(result != CURLE_OK) {
    curl_mfprintf(stderr, "curl_global_init_mem() failed\n");
    return TEST_ERR_MAJOR_BAD;
  }

  /* learn on a scratch multi which add grows the transfer table */
  multi_init(multi);
  initial_rows = t2507_rows;
  for(i = 0; i < T2507_MAX_FILL && !grow_at; i++) {
    easy_init(t2507_fill[i]);
    multi_add_handle(multi, t2507_fill[i]);
    if(t2507_rows > initial_rows) {
      grow_at = i + 1;
      grow_rows = t2507_rows;
    }
  }
  for(i = 0; i < T2507_MAX_FILL; i++) {
    if(t2507_fill[i]) {
      curl_multi_remove_handle(multi, t2507_fill[i]);
      curl_easy_cleanup(t2507_fill[i]);
      t2507_fill[i] = NULL;
    }
  }
  curl_multi_cleanup(multi);
  multi = NULL;

  curl_mfprintf(stderr, "table grows to %zu rows at add %d\n",
                grow_rows, grow_at);
  if(grow_at < 2) {
    result = TEST_ERR_MAJOR_BAD;
    goto test_cleanup;
  }

  /* fill up so that the first DoH probe is the add that grows the table */
  multi_init(multi);
  for(i = 0; i < grow_at - 2; i++) {
    easy_init(t2507_fill[i]);
    multi_add_handle(multi, t2507_fill[i]);
  }

  easy_init(curl);
  easy_setopt(curl, CURLOPT_URL, "http://host.invalid/");
  easy_setopt(curl, CURLOPT_DOH_URL, URL);
  easy_setopt(curl, CURLOPT_NOPROXY, "*");
  easy_setopt(curl, CURLOPT_TIMEOUT_MS, 10000L);
  multi_add_handle(multi, curl);

  t2507_fail_rows = grow_rows;

  multi_perform(multi, &still_running);

  abort_on_test_timeout();

  while(still_running) {
    CURLMcode mresult;
    int num;
    mresult = curl_multi_poll(multi, NULL, 0, 100, &num);
    if(mresult != CURLM_OK) {
      curl_mprintf("curl_multi_poll() returned %d\n", mresult);
      result = TEST_ERR_MAJOR_BAD;
      goto test_cleanup;
    }

    abort_on_test_timeout();

    multi_perform(multi, &still_running);

    abort_on_test_timeout();
  }

  if(t2507_fail_rows) {
    curl_mfprintf(stderr, "no DoH probe grew the transfer table\n");
    result = TEST_ERR_MAJOR_BAD;
    goto test_cleanup;
  }

  do {
    msg = curl_multi_info_read(multi, &still_running);
    if(msg && msg->msg == CURLMSG_DONE && msg->easy_handle == curl)
      xfer_result = msg->data.result;
  } while(msg);

  if(xfer_result != CURLE_OUT_OF_MEMORY) {
    curl_mfprintf(stderr, "transfer returned %d, expected %d\n",
                  (int)xfer_result, (int)CURLE_OUT_OF_MEMORY);
    result = TEST_ERR_FAILURE;
  }

test_cleanup:

  t2507_fail_rows = 0;
  curl_multi_cleanup(multi);
  curl_easy_cleanup(curl);
  for(i = 0; i < T2507_MAX_FILL; i++)
    curl_easy_cleanup(t2507_fill[i]);
  curl_global_cleanup();

  return result;
}
