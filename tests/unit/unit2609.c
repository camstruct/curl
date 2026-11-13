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
#include "sendf.h"

struct t2609_cw_ctx {
  struct Curl_cwriter super;
  BIT(seen_body);
};

/* Pauses the transfer on the first body bytes and fails a 0-length EOS. */
static CURLcode t2609_cw_write(struct Curl_easy *data,
                               struct Curl_cwriter *writer, int type,
                               const char *buf, size_t nbytes)
{
  struct t2609_cw_ctx *ctx = writer->ctx;

  (void)buf;
  if(!nbytes && (type & CLIENTWRITE_EOS))
    return CURLE_WRITE_ERROR;
  if(!ctx->seen_body && (type & CLIENTWRITE_BODY)) {
    ctx->seen_body = TRUE;
    data->req.writer.paused = TRUE;
  }
  return CURLE_OK;
}

static CURLcode test_unit2609(const char *arg)
{
  UNITTEST_BEGIN_SIMPLE

  static const struct Curl_cwtype t2609_cwt = {
    "t2609",
    NULL,
    CURL_CW_FLAG_BLOWUP,
    Curl_cwriter_def_init,
    t2609_cw_write,
    Curl_cwriter_def_flush,
    Curl_cwriter_def_close,
    sizeof(struct t2609_cw_ctx)
  };
  struct Curl_easy *data;
  struct Curl_cwriter *writer = NULL;
  CURLcode result;

  if(curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
    curl_mfprintf(stderr, "curl_global_init() failed\n");
    return TEST_ERR_MAJOR_BAD;
  }

  data = curl_easy_init();
  if(!data) {
    curl_global_cleanup();
    return TEST_ERR_MAJOR_BAD;
  }
  data->req.maxdownload = -1;
  data->req.size = -1;

  result = Curl_cwriter_create(&writer, data, &t2609_cwt,
                               CURL_CW_CONTENT_DECODE);
  if(!result) {
    result = Curl_cwriter_add(data, writer);
    if(result)
      Curl_cwriter_free(data, writer);
  }
  if(result) {
    curl_easy_cleanup(data);
    curl_global_cleanup();
    return TEST_ERR_MAJOR_BAD;
  }

  result = Curl_client_write(data, CLIENTWRITE_BODY, "a", 1);
  fail_unless(result == CURLE_OK, "first body write failed");
  fail_unless(Curl_cwriter_is_paused(data), "writer is not paused");

  result = Curl_client_write(data, CLIENTWRITE_BODY | CLIENTWRITE_EOS, "", 0);
  fail_unless(result == CURLE_OK, "paused EOS write was not buffered");
  result = Curl_client_write(data, CLIENTWRITE_BODY, "b", 1);
  fail_unless(result == CURLE_OK, "paused body write was not buffered");

  result = Curl_cwriter_unpause(data);
  fail_unless(result == CURLE_WRITE_ERROR,
              "unpause did not return the failed EOS write");

  curl_easy_cleanup(data);
  curl_global_cleanup();

  UNITTEST_END_SIMPLE
}
