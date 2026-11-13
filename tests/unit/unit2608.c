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
#include "cfilters.h"
#include "curl_trc.h"
#include "transfer.h"

/* Curl_xfer_pause_recv() must report the result of the connection
 * filters' CF_CTRL_DATA_PAUSE event. */

struct t2608_ctx {
  int pause_calls;
  CURLcode pause_result;
};

static void t2608_cf_destroy(struct Curl_cfilter *cf,
                             struct Curl_easy *data)
{
  (void)data;
  /* ctx is owned by the test case, nothing to free */
  cf->ctx = NULL;
}

static CURLcode t2608_cf_connect(struct Curl_cfilter *cf,
                                 struct Curl_easy *data,
                                 bool *done)
{
  (void)cf;
  (void)data;
  *done = TRUE;
  return CURLE_OK;
}

static CURLcode t2608_cf_cntrl(struct Curl_cfilter *cf,
                               struct Curl_easy *data,
                               int event, int arg1, void *arg2)
{
  struct t2608_ctx *ctx = cf->ctx;
  (void)data;
  (void)arg1;
  (void)arg2;
  if(event == CF_CTRL_DATA_PAUSE) {
    ctx->pause_calls++;
    return ctx->pause_result;
  }
  return CURLE_OK;
}

static const struct Curl_cftype t2608_cft = {
  "TEST-PAUSE",
  CF_TYPE_IP_CONNECT,
  CURL_LOG_LVL_NONE,
  t2608_cf_destroy,
  t2608_cf_connect,
  Curl_cf_def_shutdown,
  Curl_cf_def_adjust_pollset,
  Curl_cf_def_data_pending,
  Curl_cf_def_send,
  Curl_cf_def_recv,
  t2608_cf_cntrl,
  Curl_cf_def_conn_is_alive,
  Curl_cf_def_conn_keep_alive,
  Curl_cf_def_query,
};

static CURLcode t2608_setup(CURL **easy)
{
  CURLcode result = CURLE_OK;

  global_init(CURL_GLOBAL_ALL);
  *easy = curl_easy_init();
  if(!*easy) {
    curl_global_cleanup();
    return CURLE_OUT_OF_MEMORY;
  }
  return result;
}

static void t2608_stop(CURL *easy)
{
  curl_easy_cleanup(easy);
  curl_global_cleanup();
}

static CURLcode test_unit2608(const char *arg)
{
  CURL *easy;

  UNITTEST_BEGIN(t2608_setup(&easy))

  struct Curl_easy *data = easy;
  struct connectdata *conn;
  struct Curl_cfilter *cf = NULL;
  struct t2608_ctx ctx;
  CURLcode result;

  conn = curlx_calloc(1, sizeof(*conn));
  abort_if(!conn, "could not allocate a connection");

  memset(&ctx, 0, sizeof(ctx));
  result = Curl_cf_create(&cf, &t2608_cft, &ctx);
  fail_unless(!result, "creating the filter failed");
  if(!result) {
    Curl_conn_cf_add(data, conn, FIRSTSOCKET, cf);
    cf->connected = TRUE;
    data->conn = conn;

    result = Curl_xfer_pause_recv(data, TRUE);
    fail_unless(ctx.pause_calls == 1, "filter did not see the pause");
    fail_unless(!result, "pause failed with a succeeding filter");

    ctx.pause_result = CURLE_HTTP2;
    result = Curl_xfer_pause_recv(data, FALSE);
    fail_unless(ctx.pause_calls == 2, "filter did not see the unpause");
    fail_unless(result == CURLE_HTTP2,
                "unpause did not return the filter's error");

    result = Curl_xfer_pause_recv(data, TRUE);
    fail_unless(ctx.pause_calls == 3, "filter did not see the 2nd pause");
    fail_unless(result == CURLE_HTTP2,
                "pause did not return the filter's error");

    data->conn = NULL;
    Curl_conn_cf_discard_all(data, conn, FIRSTSOCKET);
  }
  curlx_free(conn);

  UNITTEST_END(t2608_stop(easy))
}
