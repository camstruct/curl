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

#define T1600_TRANSFERS  2
#define T1600_TIMEOUT_MS 1000L
#define T1600_LOCK_MS    3000

struct t1600_locks {
  int locks;
  int unlocks;
};

static void t1600_test_lock(CURL *curl, curl_lock_data data,
                            curl_lock_access laccess, void *useptr)
{
  struct t1600_locks *l = (struct t1600_locks *)useptr;
  (void)curl;
  (void)laccess;
  if(data != CURL_LOCK_DATA_DNS)
    return;
  /* outlast the resolve timeout while the first lock is being taken */
  if(!l->locks++)
    curlx_wait_ms(T1600_LOCK_MS);
}

static void t1600_test_unlock(CURL *curl, curl_lock_data data, void *useptr)
{
  struct t1600_locks *l = (struct t1600_locks *)useptr;
  (void)curl;
  if(data == CURL_LOCK_DATA_DNS)
    l->unlocks++;
}

static CURLcode test_lib1600(const char *URL)
{
  CURLcode result = CURLE_OK;
  CURLSH *share;
  struct t1600_locks l = { 0, 0 };
  int i;

  curl_global_init(CURL_GLOBAL_ALL);

  share = curl_share_init();
  if(!share) {
    curl_mfprintf(stderr, "curl_share_init() failed\n");
    result = TEST_ERR_MAJOR_BAD;
    goto test_cleanup;
  }
  curl_share_setopt(share, CURLSHOPT_LOCKFUNC, t1600_test_lock);
  curl_share_setopt(share, CURLSHOPT_UNLOCKFUNC, t1600_test_unlock);
  curl_share_setopt(share, CURLSHOPT_USERDATA, (void *)&l);
  curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);

  for(i = 0; i < T1600_TRANSFERS; i++) {
    CURL *curl = curl_easy_init();
    if(!curl) {
      curl_mfprintf(stderr, "curl_easy_init() failed\n");
      result = TEST_ERR_MAJOR_BAD;
      goto test_cleanup;
    }
    curl_easy_setopt(curl, CURLOPT_URL, URL);
    curl_easy_setopt(curl, CURLOPT_SHARE, share);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, T1600_TIMEOUT_MS);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    /* the name does not resolve, only the share locking matters */
    (void)curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if(l.locks != l.unlocks) {
      curl_mfprintf(stderr, "transfer %d: %d locks, %d unlocks\n",
                    i, l.locks, l.unlocks);
      result = TEST_ERR_FAILURE;
      goto test_cleanup;
    }
  }

test_cleanup:
  if(share)
    curl_share_cleanup(share);
  curl_global_cleanup();
  return result;
}
