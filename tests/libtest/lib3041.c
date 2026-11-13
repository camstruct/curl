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

#define T3041_THREADS 4
#define T3041_LOOPS   5000

struct t3041_seen {
  size_t names;
  int features;
  CURLcode result;
};

static void t3041_version_info(struct t3041_seen *got)
{
  curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);
  const char * const *name;

  got->names = 0;
  for(name = ver->feature_names; *name; name++)
    got->names++;
  got->features = ver->features;
}

#ifdef HAVE_THREADS_POSIX

static struct t3041_seen t3041_expected;

static void *t3041_run_thread(void *ptr)
{
  struct t3041_seen *got = ptr;
  int i;

  got->result = CURLE_OK;
  for(i = 0; i < T3041_LOOPS; i++) {
    t3041_version_info(got);
    if(got->names != t3041_expected.names ||
       got->features != t3041_expected.features) {
      got->result = TEST_ERR_FAILURE;
      break;
    }
  }

  return NULL;
}

static CURLcode test_lib3041(const char *URL)
{
  struct t3041_seen got[T3041_THREADS];
  pthread_t tids[T3041_THREADS];
  unsigned tid_count = T3041_THREADS, i;
  CURLcode result = CURLE_OK;
  (void)URL;

  t3041_version_info(&t3041_expected);

  for(i = 0; i < tid_count; i++) {
    int res = pthread_create(&tids[i], NULL, t3041_run_thread, &got[i]);
    if(res) {
      curl_mfprintf(stderr, "%s:%d Could not create thread, errno %d\n",
                    __FILE__, __LINE__, res);
      tid_count = i;
      result = TEST_ERR_MAJOR_BAD;
      goto cleanup;
    }
  }

cleanup:
  for(i = 0; i < tid_count; i++) {
    pthread_join(tids[i], NULL);
    if(got[i].result != CURLE_OK) {
      curl_mfprintf(stderr, "%s:%d thread[%u]: curl_version_info() gave "
                    "%zu feature names and features 0x%x, expected "
                    "%zu and 0x%x\n", __FILE__, __LINE__, i,
                    got[i].names, (unsigned int)got[i].features,
                    t3041_expected.names,
                    (unsigned int)t3041_expected.features);
      result = TEST_ERR_MAJOR_BAD;
    }
  }

  return result;
}

#else /* without pthread, this test does not work */
static CURLcode test_lib3041(const char *URL)
{
  struct t3041_seen first;
  struct t3041_seen second;
  (void)URL;

  t3041_version_info(&first);
  t3041_version_info(&second);
  if(first.names != second.names || first.features != second.features)
    return TEST_ERR_MAJOR_BAD;
  return CURLE_OK;
}
#endif
