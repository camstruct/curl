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
#include "curl_setup.h"
#include "vtls/keylog.h"

#if defined(USE_OPENSSL) || defined(USE_GNUTLS) || defined(USE_WOLFSSL) || \
  defined(USE_RUSTLS)

#include "escape.h"
#include "curlx/win32-fopen.h"
#include "easy_lock.h"
/* The fp for the open SSLKEYLOGFILE, or NULL if not open */
static FILE *keylog_file_fp;
/* Used for verbose logging */
static char *keylog_file_name;

static void keylog_open_locked(void)
{
  if(!keylog_file_fp) {
    keylog_file_name = getenv("SSLKEYLOGFILE");
    if(keylog_file_name) {
      keylog_file_fp = curlx_fopen(keylog_file_name, FOPEN_APPENDTEXT);
      if(keylog_file_fp) {
#ifdef _WIN32
        if(setvbuf(keylog_file_fp, NULL, _IONBF, 0))
#else
        if(setvbuf(keylog_file_fp, NULL, _IOLBF, 4096))
#endif
        {
          curlx_fclose(keylog_file_fp);
          keylog_file_fp = NULL;
        }
      }
    }
  }
}

#ifdef GLOBAL_INIT_IS_THREADSAFE
/* Protects keylog_file_fp and keylog_file_name */
static curl_simple_lock s_keylog_lock = CURL_SIMPLE_LOCK_INIT;
#define keylog_lock()   curl_simple_lock_lock(&s_keylog_lock)
#define keylog_unlock() curl_simple_lock_unlock(&s_keylog_lock)
#else
#define keylog_lock()
#define keylog_unlock()
#endif

static FILE *keylog_fp(void)
{
  FILE *fp;

  keylog_lock();
  fp = keylog_file_fp;
  keylog_unlock();
  return fp;
}

void Curl_tls_keylog_open(void)
{
  keylog_lock();
  keylog_open_locked();
  keylog_unlock();
}

void Curl_tls_keylog_close(void)
{
  FILE *fp;

  keylog_lock();
  fp = keylog_file_fp;
  keylog_file_fp = NULL;
  keylog_unlock();
  if(fp)
    curlx_fclose(fp);
}

bool Curl_tls_keylog_enabled(void)
{
  return !!keylog_fp();
}

const char *Curl_tls_keylog_file_name(void)
{
  const char *name;

  keylog_lock();
  name = keylog_file_name;
  keylog_unlock();
  return name;
}

bool Curl_tls_keylog_write_line(const char *line)
{
  /* The current maximum valid keylog line length LF and NUL is 195. */
  size_t linelen;
  char buf[256];
  FILE *fp = keylog_fp();

  if(!fp || !line) {
    return FALSE;
  }

  linelen = strlen(line);
  if(linelen == 0 || linelen > sizeof(buf) - 2) {
    /* Empty line or too big to fit in an LF and NUL. */
    return FALSE;
  }

  memcpy(buf, line, linelen);
  if(line[linelen - 1] != '\n') {
    buf[linelen++] = '\n';
  }
  buf[linelen] = '\0';

  /* Using fputs here instead of fprintf since libcurl's fprintf replacement
     may not be thread-safe. */
  fputs(buf, fp);
  return TRUE;
}

bool Curl_tls_keylog_write(const char *label,
                           const unsigned char *client_random,
                           size_t random_size,
                           const unsigned char *secret, size_t secretlen)
{
  size_t pos, i;
  unsigned char line[KEYLOG_LABEL_MAXLEN + 1 +
                     (2 * CLIENT_RANDOM_SIZE) + 1 +
                     (2 * SECRET_MAXLEN) + 1 + 1];
  FILE *fp = keylog_fp();
  DEBUGASSERT(random_size >= CLIENT_RANDOM_SIZE);
  if(random_size < CLIENT_RANDOM_SIZE)
    return FALSE;
  if(!fp)
    return FALSE;

  pos = strlen(label);
  if(pos > KEYLOG_LABEL_MAXLEN || !secretlen || secretlen > SECRET_MAXLEN) {
    /* Should never happen - sanity check anyway. */
    return FALSE;
  }

  memcpy(line, label, pos);
  line[pos++] = ' ';

  /* Client Random */
  for(i = 0; i < CLIENT_RANDOM_SIZE; i++) {
    Curl_hexbyte(&line[pos], client_random[i]);
    pos += 2;
  }
  line[pos++] = ' ';

  /* Secret */
  for(i = 0; i < secretlen; i++) {
    Curl_hexbyte(&line[pos], secret[i]);
    pos += 2;
  }
  line[pos++] = '\n';
  line[pos] = '\0';

  /* Using fputs here instead of fprintf since libcurl's fprintf replacement
     may not be thread-safe. */
  fputs((char *)line, fp);
  return TRUE;
}

#else /* TLS backend */

bool Curl_tls_keylog_enabled(void)
{
  return FALSE;
}

const char *Curl_tls_keylog_file_name(void)
{
  return NULL;
}

#endif  /* TLS backend */
