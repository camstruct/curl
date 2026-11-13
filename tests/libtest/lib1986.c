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
 * Do not use directly memory related functions in this file except those used
 * inside custom memory callbacks which should be calling 'the real thing'.
 */

static bool t1986_fail_strdup;

static void *t1986_calloc(size_t nmemb, size_t size)
{
  /* !checksrc! disable BANNEDFUNC 1 */
  return calloc(nmemb, size);
}

static void *t1986_malloc(size_t size)
{
  /* debug builds duplicate strings through the malloc callback */
  if(t1986_fail_strdup) {
    t1986_fail_strdup = FALSE;
    return NULL;
  }
  /* !checksrc! disable BANNEDFUNC 1 */
  return malloc(size);
}

static char *t1986_strdup(const char *ptr)
{
  if(t1986_fail_strdup) {
    t1986_fail_strdup = FALSE;
    return NULL;
  }
  return CURLX_STRDUP_LOW(ptr);
}

static void *t1986_realloc(void *ptr, size_t size)
{
  /* !checksrc! disable BANNEDFUNC 1 */
  return realloc(ptr, size);
}

static void t1986_free(void *ptr)
{
  /* !checksrc! disable BANNEDFUNC 1 */
  free(ptr);
}

static CURLcode test_lib1986(const char *URL)
{
  CURLcode result;
  CURL *curl;

  result = curl_global_init_mem(CURL_GLOBAL_ALL,
                                t1986_malloc,
                                t1986_free,
                                t1986_realloc,
                                t1986_strdup,
                                t1986_calloc);
  if(result != CURLE_OK) {
    curl_mfprintf(stderr, "curl_global_init_mem() failed\n");
    return TEST_ERR_MAJOR_BAD;
  }

  curl = curl_easy_init();
  if(!curl) {
    curl_mfprintf(stderr, "curl_easy_init() failed\n");
    curl_global_cleanup();
    return TEST_ERR_MAJOR_BAD;
  }

  easy_setopt(curl, CURLOPT_URL, URL);
  easy_setopt(curl, CURLOPT_HEADER, 1L);
  easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, "aaaa");

  t1986_fail_strdup = TRUE;
  result = curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, "bbbb");
  t1986_fail_strdup = FALSE;
  if(result != CURLE_OUT_OF_MEMORY) {
    curl_mfprintf(stderr, "CURLOPT_COPYPOSTFIELDS returned %d, expected %d\n",
                  (int)result, (int)CURLE_OUT_OF_MEMORY);
    result = TEST_ERR_FAILURE;
    goto test_cleanup;
  }

  /* the failed option leaves the previously copied body in place */
  result = curl_easy_perform(curl);

test_cleanup:

  curl_easy_cleanup(curl);
  curl_global_cleanup();

  return result;
}
