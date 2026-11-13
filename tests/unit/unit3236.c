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
#include "mime.h"
#include "sendf.h"

#if !defined(CURL_DISABLE_MIME) && (!defined(CURL_DISABLE_HTTP) || \
    !defined(CURL_DISABLE_SMTP) || !defined(CURL_DISABLE_IMAP))

static size_t t3236_read(char *buffer, size_t size, size_t nitems, void *arg)
{
  (void)buffer;
  (void)size;
  (void)nitems;
  (void)arg;
  return 0;
}

static CURLcode t3236_setup(void)
{
  CURLcode result = CURLE_OK;
  global_init(CURL_GLOBAL_ALL);
  return result;
}

static void t3236_stop(CURL *easy, curl_mime *mime, curl_mime *subparts)
{
  if(easy)
    curl_easy_cleanup(easy);
  curl_mime_free(subparts);
  curl_mime_free(mime);
  curl_global_cleanup();
}

static CURLcode test_unit3236(const char *arg)
{
  CURL *easy = NULL;
  curl_mime *mime = NULL;
  curl_mime *subparts = NULL;
  curl_mimepart *part;
  curl_mimepart *multipart;

  UNITTEST_BEGIN(t3236_setup())

  easy = curl_easy_init();
  abort_unless(easy, "curl_easy_init()");
  mime = curl_mime_init(easy);
  abort_unless(mime, "curl_mime_init()");

  /* A known data size gets the CRLF after headers added. */
  part = curl_mime_addpart(mime);
  abort_unless(part, "curl_mime_addpart()");
  curl_mime_data_cb(part, 10, t3236_read, NULL, NULL, NULL);
  fail_unless(Curl_creader_set_mime(easy, part) == CURLE_OK,
              "small part not accepted");
  fail_unless(Curl_creader_total_length(easy) == 12,
              "wrong size for small part");

  /* The largest data size that still fits with the CRLF. */
  part = curl_mime_addpart(mime);
  abort_unless(part, "curl_mime_addpart()");
  curl_mime_data_cb(part, CURL_OFF_T_MAX - 2, t3236_read, NULL, NULL, NULL);
  fail_unless(Curl_creader_set_mime(easy, part) == CURLE_OK,
              "largest part not accepted");
  fail_unless(Curl_creader_total_length(easy) == CURL_OFF_T_MAX,
              "wrong size for largest part");

  /* A part size that cannot be represented is unknown. */
  part = curl_mime_addpart(mime);
  abort_unless(part, "curl_mime_addpart()");
  curl_mime_data_cb(part, CURL_OFF_T_MAX - 1, t3236_read, NULL, NULL, NULL);
  fail_unless(Curl_creader_set_mime(easy, part) == CURLE_OK,
              "too large part not accepted");
  fail_unless(Curl_creader_total_length(easy) == -1,
              "too large part size is not unknown");

  /* The same when the boundaries of the enclosing multipart do not fit. */
  subparts = curl_mime_init(easy);
  abort_unless(subparts, "curl_mime_init()");
  part = curl_mime_addpart(subparts);
  abort_unless(part, "curl_mime_addpart()");
  curl_mime_data_cb(part, CURL_OFF_T_MAX - 70, t3236_read, NULL, NULL, NULL);
  multipart = curl_mime_addpart(mime);
  abort_unless(multipart, "curl_mime_addpart()");
  abort_unless(curl_mime_subparts(multipart, subparts) == CURLE_OK,
               "curl_mime_subparts()");
  subparts = NULL;
  fail_unless(Curl_creader_set_mime(easy, multipart) == CURLE_OK,
              "too large multipart not accepted");
  fail_unless(Curl_creader_total_length(easy) == -1,
              "too large multipart size is not unknown");

  UNITTEST_END(t3236_stop(easy, mime, subparts))
}

#else /* mime disabled */

static CURLcode test_unit3236(const char *arg)
{
  UNITTEST_BEGIN_SIMPLE
  puts("nothing to do when mime is disabled");
  UNITTEST_END_SIMPLE
}

#endif
