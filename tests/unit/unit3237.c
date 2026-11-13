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

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#include <signal.h>

#if !defined(CURL_DISABLE_HTTP) && !defined(CURL_DISABLE_HSTS) && \
  defined(HAVE_SETRLIMIT) && defined(RLIMIT_FSIZE) && defined(SIGXFSZ)
#include "urldata.h"
#include "hsts.h"

static CURLcode test_unit3237(const char *arg)
{
  UNITTEST_BEGIN_SIMPLE

  CURLcode result;
  struct hsts *h = Curl_hsts_init();
  CURL *easy;
  struct rlimit saved;
  struct rlimit limit;

  abort_unless(h, "Curl_hsts_init()");

  curl_global_init(CURL_GLOBAL_ALL);
  easy = curl_easy_init();
  if(!easy) {
    Curl_hsts_cleanup(&h);
    curl_global_cleanup();
    abort_unless(easy, "curl_easy_init()");
  }

  Curl_hsts_loadfile(easy, h, arg);
  fail_unless(Curl_llist_count(&h->list) == 3, "cache file not loaded");

  /* make writing the cache fail part way through, like on a full disk */
  signal(SIGXFSZ, SIG_IGN);
  if(getrlimit(RLIMIT_FSIZE, &saved)) {
    Curl_hsts_cleanup(&h);
    curl_easy_cleanup(easy);
    curl_global_cleanup();
    abort_unless(FALSE, "getrlimit()");
  }
  limit = saved;
  limit.rlim_cur = 64;
  setrlimit(RLIMIT_FSIZE, &limit);

  result = Curl_hsts_save(easy, h, arg);

  setrlimit(RLIMIT_FSIZE, &saved);
  signal(SIGXFSZ, SIG_DFL);

  fail_unless(result == CURLE_WRITE_ERROR,
              "failed save did not return CURLE_WRITE_ERROR");

  Curl_hsts_cleanup(&h);
  curl_easy_cleanup(easy);
  curl_global_cleanup();

  UNITTEST_END(curl_global_cleanup())
}
#else
static CURLcode test_unit3237(const char *arg)
{
  UNITTEST_BEGIN_SIMPLE
  puts("nothing to do without HSTS or file size limits");
  UNITTEST_END_SIMPLE
}
#endif
