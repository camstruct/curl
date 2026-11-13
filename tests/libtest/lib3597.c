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
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF
 * ANY KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/
#include "unitcheck.h"

#if !defined(CURL_DISABLE_MIME) && !defined(CURL_DISABLE_HTTP)
static size_t oversized_read(char *buffer, size_t size, size_t nitems,
                             void *arg)
{
  (void)arg;
  memset(buffer, 'A', size * nitems);
  return (size * nitems) + 4096;
}
#endif

static CURLcode test_lib3597(const char *arg)
{
  UNITTEST_BEGIN_SIMPLE

#if !defined(CURL_DISABLE_MIME) && !defined(CURL_DISABLE_HTTP)

  CURL *curl = NULL;
  CURLcode result = CURLE_OK;
  curl_mime *mime = NULL;
  curl_mimepart *part = NULL;

  if(curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
    curl_mfprintf(stderr, "curl_global_init() failed\n");
    return TEST_ERR_MAJOR_BAD;
  }

  curl = curl_easy_init();
  fail_unless(curl != NULL, "curl_easy_init() failed");

  mime = curl_mime_init(curl);
  fail_unless(mime != NULL, "curl_mime_init failed");
  part = curl_mime_addpart(mime);
  fail_unless(part != NULL, "curl_mime_addpart failed");
  result = curl_mime_data_cb(part, -1, oversized_read, NULL, NULL, NULL);
  fail_unless(result == CURLE_OK, "curl_mime_data_cb failed");
  result = curl_mime_encoder(part, "base64");
  fail_unless(result == CURLE_OK, "curl_mime_encoder failed");

  result = curl_easy_setopt(curl, CURLOPT_URL, arg);
  fail_unless(result == CURLE_OK, "CURLOPT_URL failed");
  result = curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
  fail_unless(result == CURLE_OK, "CURLOPT_MIMEPOST failed");

  result = curl_easy_perform(curl);
  fail_unless(result != CURLE_OK,
              "oversized read callback return value was accepted");

  curl_easy_cleanup(curl);
  curl_mime_free(mime);
  curl_global_cleanup();
#else
  (void)arg;
#endif

  UNITTEST_END_SIMPLE
}
