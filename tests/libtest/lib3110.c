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

static int t3110_save_failures;

static int t3110_debug_cb(CURL *curl, curl_infotype type,
                          char *data, size_t size, void *userp)
{
  static const char warning[] = "WARNING: failed to save cookies";
  size_t len = sizeof(warning) - 1;
  (void)curl;
  (void)userp;

  if(type == CURLINFO_TEXT && size >= len && !memcmp(data, warning, len))
    t3110_save_failures++;
  return 0;
}

static CURLcode test_lib3110(const char *URL)
{
  CURLcode result = CURLE_OK;
  CURL *curl;

  curl_global_init(CURL_GLOBAL_ALL);

  curl = curl_easy_init();
  easy_setopt(curl, CURLOPT_DEBUGFUNCTION, t3110_debug_cb);
  easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  easy_setopt(curl, CURLOPT_URL, URL);

  /* a jar that accepts the open but fails every write */
  easy_setopt(curl, CURLOPT_COOKIEJAR, libtest_arg2);
  easy_setopt(curl, CURLOPT_COOKIELIST,
              "Set-Cookie: c1=v1; domain=example.com");

  result = curl_easy_perform(curl);
  if(result) {
    curl_mfprintf(stderr, "curl_easy_perform() failed: %s\n",
                  curl_easy_strerror(result));
    goto test_cleanup;
  }

  easy_setopt(curl, CURLOPT_COOKIELIST, "FLUSH");

test_cleanup:

  /* always cleanup */
  curl_easy_cleanup(curl);
  curl_mprintf("cookie save failures: %d\n", t3110_save_failures);
  curl_global_cleanup();

  return result;
}
