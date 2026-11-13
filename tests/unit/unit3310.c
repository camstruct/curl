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
#include "urldata.h"

#ifdef USE_SSL
#include "vtls/vtls.h"
#include "vtls/vtls_scache.h"

static int t3310_held[CURL_LOCK_DATA_LAST];

static void t3310_lock(CURL *handle, curl_lock_data data,
                       curl_lock_access laccess, void *useptr)
{
  (void)handle;
  (void)laccess;
  (void)useptr;
  t3310_held[data]++;
}

static void t3310_unlock(CURL *handle, curl_lock_data data, void *useptr)
{
  (void)handle;
  (void)useptr;
  t3310_held[data]--;
}
#endif

static CURLcode t3310_setup(void)
{
  CURLcode result = CURLE_OK;
  global_init(CURL_GLOBAL_ALL);
  return result;
}

static CURLcode test_unit3310(const char *arg)
{
  UNITTEST_BEGIN(t3310_setup())

#ifdef USE_SSL
  CURLSH *share;
  CURL *curl = NULL;
  CURLM *multi;
  CURLcode result;

  share = curl_share_init();
  abort_unless(share, "curl_share_init() failed");
  curl_share_setopt(share, CURLSHOPT_LOCKFUNC, t3310_lock);
  curl_share_setopt(share, CURLSHOPT_UNLOCKFUNC, t3310_unlock);
  curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);

  curl = curl_easy_init();
  if(!curl) {
    curl_share_cleanup(share);
    unittest_abort("curl_easy_init() failed");
  }
  result = curl_easy_setopt(curl, CURLOPT_SHARE, share);
  fail_unless(result == CURLE_OK, "setting the share failed");

  Curl_ssl_scache_lock((struct Curl_easy *)curl);
  fail_unless(t3310_held[CURL_LOCK_DATA_SSL_SESSION] == 1,
              "session cache lock should be held");

  result = curl_easy_setopt(curl, CURLOPT_SHARE, NULL);
  fail_unless(result == CURLE_BAD_FUNCTION_ARGUMENT,
              "share must not change while its session cache is locked");

  Curl_ssl_scache_unlock((struct Curl_easy *)curl);
  fail_unless(t3310_held[CURL_LOCK_DATA_SSL_SESSION] == 0,
              "session cache lock should be released");

  result = curl_easy_setopt(curl, CURLOPT_SHARE, NULL);
  fail_unless(result == CURLE_OK, "removing the share failed");

  multi = curl_multi_init();
  if(multi) {
    curl_multi_add_handle(multi, curl);
    Curl_ssl_scache_lock((struct Curl_easy *)curl);
    curl_multi_remove_handle(multi, curl);
    Curl_ssl_scache_unlock((struct Curl_easy *)curl);

    curl_multi_add_handle(multi, curl);
    fail_if(Curl_ssl_scache_is_locked_by_current_thread(
              (struct Curl_easy *)curl),
            "session cache of the multi should be unlocked");
    curl_multi_remove_handle(multi, curl);
    curl_multi_cleanup(multi);
  }
  else
    fail("curl_multi_init() failed");

  curl_easy_cleanup(curl);
  curl_share_cleanup(share);
#endif /* USE_SSL */

  UNITTEST_END(curl_global_cleanup())
}
