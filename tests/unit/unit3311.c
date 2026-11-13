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
#include "vtls/keylog.h"

#if defined(HAVE_THREADS_POSIX) && \
  (defined(USE_OPENSSL) || defined(USE_GNUTLS) || defined(USE_WOLFSSL) || \
   defined(USE_RUSTLS))

#define T3311_THREADS 4

static void *t3311_close_thread(void *ptr)
{
  (void)ptr;
  Curl_tls_keylog_close();
  return NULL;
}

static CURLcode test_unit3311(const char *arg)
{
  UNITTEST_BEGIN_SIMPLE

  pthread_t tids[T3311_THREADS];
  unsigned int tid_count = T3311_THREADS, i;

  abort_unless(!setenv("SSLKEYLOGFILE", arg, 1), "setenv failed");

  Curl_tls_keylog_open();
  abort_unless(Curl_tls_keylog_enabled(), "keylog file not opened");

  for(i = 0; i < tid_count; i++) {
    if(pthread_create(&tids[i], NULL, t3311_close_thread, NULL)) {
      curl_mfprintf(stderr, "%s:%d Could not create thread\n",
                    __FILE__, __LINE__);
      tid_count = i;
      unitfail++;
      break;
    }
  }
  for(i = 0; i < tid_count; i++)
    pthread_join(tids[i], NULL);

  fail_if(Curl_tls_keylog_enabled(), "keylog file still open after close");

  Curl_tls_keylog_open();
  fail_unless(Curl_tls_keylog_enabled(), "keylog file not reopened");
  fail_unless(Curl_tls_keylog_write_line("# unit3311"),
              "keylog line not written");
  Curl_tls_keylog_close();
  fail_if(Curl_tls_keylog_enabled(), "keylog file still open after close");

  UNITTEST_END_SIMPLE
}

#else

static CURLcode test_unit3311(const char *arg)
{
  UNITTEST_BEGIN_SIMPLE
  puts("nothing to do without POSIX threads and a keylog TLS backend");
  UNITTEST_END_SIMPLE
}

#endif
