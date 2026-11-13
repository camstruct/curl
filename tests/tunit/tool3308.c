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
#include "tool_setup.h"
#include "slist_wc.h"
#include "tool_easysrc.h"

static bool t3308_fail_append;

/* replaces the libcurl function in this executable so that the allocation
   failure can be triggered on demand */
struct curl_slist *curl_slist_append(struct curl_slist *list,
                                     const char *data)
{
  struct curl_slist *item;
  struct curl_slist *last;

  if(t3308_fail_append)
    return NULL;

  item = curlx_malloc(sizeof(*item));
  if(!item)
    return NULL;
  item->data = curlx_strdup(data);
  if(!item->data) {
    curlx_free(item);
    return NULL;
  }
  item->next = NULL;
  if(!list)
    return item;
  last = list;
  while(last->next)
    last = last->next;
  last->next = item;
  return list;
}

static CURLcode test_tool3308(const char *arg)
{
  UNITTEST_BEGIN_SIMPLE

#ifndef CURL_DISABLE_LIBCURL_OPTION
  CURLcode result;

  result = easysrc_add(&easysrc_toohard, "--hard");
  abort_unless(!result, "add unconvertible option");

  t3308_fail_append = TRUE;
  result = easysrc_add(&easysrc_decl, "int i;");
  t3308_fail_append = FALSE;

  fail_unless(result == CURLE_OUT_OF_MEMORY,
              "allocation failure must be reported");
  fail_unless(easysrc_toohard != NULL,
              "unrelated source lists must stay intact");

  slist_wc_free_all(easysrc_toohard);
  easysrc_toohard = NULL;
#endif

  UNITTEST_END_SIMPLE
}
