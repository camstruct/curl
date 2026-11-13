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

struct t3235_sockets {
  CURL *curl;
  int calls;
  int repeated;
  curl_socket_t rejected_fd;
  int rejected_what;
};

static int t3235_socket_cb(CURL *curl, curl_socket_t s, int what,
                           void *clientp, void *socketp)
{
  struct t3235_sockets *socks = clientp;
  (void)socketp;

  if(curl != socks->curl)
    return 0;

  socks->calls++;
  if(socks->calls == 1) {
    socks->rejected_fd = s;
    socks->rejected_what = what;
    return -1;
  }
  if(s == socks->rejected_fd && what == socks->rejected_what)
    socks->repeated++;
  return 0;
}

static CURLcode test_lib3235(const char *URL)
{
  CURL *curl = NULL;
  CURLM *multi = NULL;
  CURLMcode mresult = CURLM_OK;
  CURLcode result = CURLE_OK;
  struct t3235_sockets socks = { NULL, 0, 0, CURL_SOCKET_BAD, 0 };
  int still_running = 0;

  start_test_timing();

  global_init(CURL_GLOBAL_ALL);

  multi_init(multi);

  easy_init(curl);
  socks.curl = curl;

  easy_setopt(curl, CURLOPT_URL, URL);

  multi_setopt(multi, CURLMOPT_SOCKETFUNCTION, t3235_socket_cb);
  multi_setopt(multi, CURLMOPT_SOCKETDATA, &socks);

  multi_add_handle(multi, curl);

  while(!socks.calls) {
    mresult = curl_multi_socket_action(multi, CURL_SOCKET_TIMEOUT, 0,
                                       &still_running);
    if(mresult)
      break;

    abort_on_test_timeout();

    curlx_wait_ms(10);
  }

  if(mresult != CURLM_ABORTED_BY_CALLBACK) {
    curl_mfprintf(stderr, "curl_multi_socket_action() returned %d, "
                  "expected %d\n", mresult, CURLM_ABORTED_BY_CALLBACK);
    result = TEST_ERR_MAJOR_BAD;
    goto test_cleanup;
  }

  curl_mfprintf(stderr, "curl_easy_pause(RECV) returned %d\n",
                curl_easy_pause(curl, CURLPAUSE_RECV));
  curl_mfprintf(stderr, "curl_easy_pause(CONT) returned %d\n",
                curl_easy_pause(curl, CURLPAUSE_CONT));

  if(socks.repeated) {
    curl_mfprintf(stderr, "rejected socket was announced again %d times\n",
                  socks.repeated);
    result = TEST_ERR_FAILURE;
  }

test_cleanup:

  curl_multi_remove_handle(multi, curl);
  curl_easy_cleanup(curl);
  curl_multi_cleanup(multi);
  curl_global_cleanup();

  return result;
}
