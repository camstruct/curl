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

static CURLcode test_lib2314(const char *URL)
{
#ifndef CURL_DISABLE_WEBSOCKETS
  CURL *curl;
  CURLcode result = CURLE_OK;

  global_init(CURL_GLOBAL_ALL);

  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, URL);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "websocket/2314");
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L); /* websocket style */
    result = curl_easy_perform(curl);
    curl_mfprintf(stderr, "curl_easy_perform() returned %d\n", (int)result);
    if(result == CURLE_OK) {
      char buffer[100];
      size_t sent = 0;

      memset(buffer, 'A', sizeof(buffer));
      /* a new frame of 10 bytes total cannot take 100 bytes */
      result = curl_ws_send(curl, buffer, sizeof(buffer), &sent, 10,
                            CURLWS_BINARY | CURLWS_OFFSET);
      curl_mfprintf(stderr, "curl_ws_send() returned %d, sent %zu\n",
                    (int)result, sent);
      if(sent)
        result = TEST_ERR_FAILURE;
    }

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
  return result;
#else
  (void)URL;
  curl_mfprintf(stderr, "Missing support\n");
  return CURLE_UNSUPPORTED_PROTOCOL;
#endif
}
