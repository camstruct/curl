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

#include "curl_threads.h"
#include "vtls/keylog.h"

#if defined(USE_THREADS) && \
  (defined(USE_OPENSSL) || defined(USE_GNUTLS) || defined(USE_WOLFSSL) || \
   defined(USE_RUSTLS))

#define UNIT3309_THREADS 4

static CURL_THREAD_RETURN_T CURL_STDCALL unit3309_open(void *arg)
{
  (void)arg;
  Curl_tls_keylog_open();
  return 0;
}

static CURLcode test_unit3309(const char *arg)
{
  UNITTEST_BEGIN_SIMPLE
  curl_thread_t threads[UNIT3309_THREADS];
  size_t i;

  Curl_tls_keylog_close();
  fail_if(Curl_tls_keylog_enabled(), "keylog enabled after close");

  for(i = 0; i < UNIT3309_THREADS; i++) {
    threads[i] = Curl_thread_create(unit3309_open, NULL);
    fail_if(threads[i] == curl_thread_t_null, "thread create");
  }
  for(i = 0; i < UNIT3309_THREADS; i++) {
    if(threads[i] != curl_thread_t_null)
      Curl_thread_join(&threads[i]);
  }

  fail_unless(Curl_tls_keylog_enabled(), "keylog not enabled after open");
  fail_unless(Curl_tls_keylog_write_line("unit3309 keylog line"),
              "keylog line not written");

  Curl_tls_keylog_close();
  fail_if(Curl_tls_keylog_enabled(), "keylog enabled after close");

  UNITTEST_END_SIMPLE
}

#else
static CURLcode test_unit3309(const char *arg)
{
  UNITTEST_BEGIN_SIMPLE
  UNITTEST_END_SIMPLE
}
#endif
