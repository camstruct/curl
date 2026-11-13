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
#include "tool_cfgable.h"
#include "tool_operate.h"
#include "tool_stderr.h"

static int t1726_lowest_free_fd(void)
{
  int fd = dup(STDIN_FILENO);
  if(fd >= 0)
    curlx_close(fd);
  return fd;
}

static CURLcode test_tool1726(const char *arg)
{
  UNITTEST_BEGIN_SIMPLE

  static char a_tool[] = "curl";
  static char a_q[] = "-q";
  static char a_etag[] = "--etag-save";
  static char a_data[] = "-d";
  static char a_foo[] = "foo";
  static char a_upload[] = "-T";
  static char a_url[] = "http://127.0.0.1:1/";
  char *argv[10];
  char *upload = curl_maprintf("%s", arg);
  char *etag = curl_maprintf("%s.etag", arg);
  CURLcode result;
  int before;
  int after;

  if(!upload || !etag) {
    curl_free(upload);
    curl_free(etag);
    fail("out of memory");
    return CURLE_OUT_OF_MEMORY;
  }

  argv[0] = a_tool;
  argv[1] = a_q;
  argv[2] = a_etag;
  argv[3] = etag;
  argv[4] = a_data;
  argv[5] = a_foo;
  argv[6] = a_upload;
  argv[7] = upload;
  argv[8] = a_url;
  argv[9] = NULL;

  tool_init_stderr();
  result = globalconf_init();
  fail_if(result, "globalconf_init failed");
  if(!result) {
    before = t1726_lowest_free_fd();
    result = operate(9, argv);
    after = t1726_lowest_free_fd();
    globalconf_free();

    curl_mprintf("operate returned %d, lowest free fd before %d, after %d\n",
                 (int)result, before, after);

    fail_if(result != CURLE_FAILED_INIT, "-T with -d should fail to init");
    fail_if(before < 0, "could not probe for a free file descriptor");
    fail_if(before != after, "the --etag-save file was left open");
  }

  curl_free(upload);
  curl_free(etag);

  UNITTEST_END_SIMPLE
}
