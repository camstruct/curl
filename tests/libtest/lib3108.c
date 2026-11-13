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

#ifdef USE_OPENSSL
#include <openssl/crypto.h>
#include <openssl/ssl.h>
#if !defined(LIBRESSL_VERSION_NUMBER) && !defined(OPENSSL_IS_BORINGSSL) && \
  !defined(OPENSSL_IS_AWSLC) && !defined(OPENSSL_NO_EC)
#define T3108_OPENSSL_MEM_HOOKS
#endif
#endif

#ifdef T3108_OPENSSL_MEM_HOOKS

#include <openssl/bio.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/x509v3.h>

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001
#endif

static bool t3108_armed;
static bool t3108_in_client;
static bool t3108_failed;
static int t3108_bio_count;
static int t3108_fail_at;

/*
 * Fail a single BIO_new() allocation done by libcurl after its TLS handshake
 * completed: the one numbered t3108_fail_at.
 */
static void *t3108_malloc(size_t size, const char *file, int line)
{
  (void)line;
  if(t3108_armed && t3108_in_client && file && strstr(file, "bio_lib")) {
    t3108_bio_count++;
    if(t3108_bio_count == t3108_fail_at) {
      t3108_failed = TRUE;
      return NULL;
    }
  }
  /* !checksrc! disable BANNEDFUNC 1 */
  return malloc(size);
}

static void *t3108_realloc(void *ptr, size_t size, const char *file, int line)
{
  (void)file;
  (void)line;
  /* !checksrc! disable BANNEDFUNC 1 */
  return realloc(ptr, size);
}

static void t3108_free(void *ptr, const char *file, int line)
{
  (void)file;
  (void)line;
  /* !checksrc! disable BANNEDFUNC 1 */
  free(ptr);
}

static void t3108_info_cb(const SSL *ssl, int where, int ret)
{
  (void)ssl;
  (void)ret;
  if(where & SSL_CB_HANDSHAKE_DONE)
    t3108_armed = TRUE;
}

static CURLcode t3108_sslctx_cb(CURL *curl, void *ssl_ctx, void *clientp)
{
  (void)curl;
  (void)clientp;
  SSL_CTX_set_info_callback((SSL_CTX *)ssl_ctx, t3108_info_cb);
  return CURLE_OK;
}

/* a TLS server for one connection at a time, driven from the test loop */
struct t3108_server {
  SSL_CTX *ctx;
  SSL *ssl;
  curl_socket_t listenfd;
  curl_socket_t sock;
  long port;
  bool replied;
};

static void t3108_server_disconnect(struct t3108_server *srv)
{
  if(srv->ssl) {
    SSL_free(srv->ssl);
    srv->ssl = NULL;
  }
  if(srv->sock != CURL_SOCKET_BAD) {
    sclose(srv->sock);
    srv->sock = CURL_SOCKET_BAD;
  }
  srv->replied = FALSE;
}

static void t3108_server_stop(struct t3108_server *srv)
{
  t3108_server_disconnect(srv);
  if(srv->listenfd != CURL_SOCKET_BAD) {
    sclose(srv->listenfd);
    srv->listenfd = CURL_SOCKET_BAD;
  }
  SSL_CTX_free(srv->ctx);
  srv->ctx = NULL;
}

static bool t3108_add_ext(X509 *cert, int nid, const char *value)
{
  X509V3_CTX v3ctx;
  X509_EXTENSION *ext;
  bool ok;

  X509V3_set_ctx_nodb(&v3ctx);
  X509V3_set_ctx(&v3ctx, cert, cert, NULL, NULL, 0);
  ext = X509V3_EXT_conf_nid(NULL, &v3ctx, nid, value);
  if(!ext)
    return FALSE;
  ok = X509_add_ext(cert, ext, -1) == 1;
  X509_EXTENSION_free(ext);
  return ok;
}

/* a self-signed certificate carrying several X509v3 extensions */
static X509 *t3108_make_cert(EVP_PKEY **pkeyp)
{
  EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
  EVP_PKEY *pkey = NULL;
  X509 *cert = NULL;
  X509_NAME *name;

  if(!pctx || EVP_PKEY_keygen_init(pctx) <= 0 ||
     EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx,
                                            NID_X9_62_prime256v1) <= 0 ||
     EVP_PKEY_keygen(pctx, &pkey) <= 0)
    goto fail;

  cert = X509_new();
  if(!cert)
    goto fail;
  name = X509_get_subject_name(cert);

  if(!X509_set_version(cert, 2) ||
     !ASN1_INTEGER_set(X509_get_serialNumber(cert), 1) ||
     !X509_gmtime_adj(X509_getm_notBefore(cert), -3600) ||
     !X509_gmtime_adj(X509_getm_notAfter(cert), 3600) ||
     !X509_set_pubkey(cert, pkey) ||
     !X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                 (const unsigned char *)"localhost",
                                 -1, -1, 0) ||
     !X509_set_issuer_name(cert, name) ||
     !t3108_add_ext(cert, NID_basic_constraints, "CA:FALSE") ||
     !t3108_add_ext(cert, NID_key_usage, "digitalSignature") ||
     !t3108_add_ext(cert, NID_subject_alt_name, "IP:127.0.0.1") ||
     !X509_sign(cert, pkey, EVP_sha256()))
    goto fail;

  EVP_PKEY_CTX_free(pctx);
  *pkeyp = pkey;
  return cert;

fail:
  X509_free(cert);
  EVP_PKEY_free(pkey);
  EVP_PKEY_CTX_free(pctx);
  return NULL;
}

static bool t3108_server_start(struct t3108_server *srv)
{
  struct sockaddr_in addr;
  curl_socklen_t addrlen = sizeof(addr);
  EVP_PKEY *pkey = NULL;
  X509 *cert;
  bool ok;

  srv->ssl = NULL;
  srv->listenfd = CURL_SOCKET_BAD;
  srv->sock = CURL_SOCKET_BAD;
  srv->port = 0;
  srv->replied = FALSE;

  srv->ctx = SSL_CTX_new(TLS_server_method());
  if(!srv->ctx)
    return FALSE;

  cert = t3108_make_cert(&pkey);
  if(!cert)
    return FALSE;
  /* up to TLS 1.2 the server has nothing left to send once the client
     considers the handshake done, so it never writes to a client that gave
     up */
  ok = SSL_CTX_set_max_proto_version(srv->ctx, TLS1_2_VERSION) == 1 &&
       SSL_CTX_use_certificate(srv->ctx, cert) == 1 &&
       SSL_CTX_use_PrivateKey(srv->ctx, pkey) == 1;
  X509_free(cert);
  EVP_PKEY_free(pkey);
  if(!ok)
    return FALSE;

  srv->listenfd = CURL_SOCKET(AF_INET, SOCK_STREAM, 0);
  if(srv->listenfd == CURL_SOCKET_BAD)
    return FALSE;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;

  if(bind(srv->listenfd, (struct sockaddr *)&addr, sizeof(addr)) ||
     listen(srv->listenfd, 1) ||
     getsockname(srv->listenfd, (struct sockaddr *)&addr, &addrlen)) {
    curl_mfprintf(stderr, "failed to listen on localhost\n");
    return FALSE;
  }
  srv->port = ntohs(addr.sin_port);

  return BIO_socket_nbio((int)srv->listenfd, 1) == 1;
}

/* advance the server as far as it gets without blocking */
static void t3108_server_run(struct t3108_server *srv)
{
  static const char reply[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 0\r\n"
    "Connection: close\r\n"
    "\r\n";
  char request[1024];
  int nread;

  if(!srv->ssl) {
    srv->sock = CURL_ACCEPT(srv->listenfd, NULL, NULL);
    if(srv->sock == CURL_SOCKET_BAD)
      return;
    srv->ssl = SSL_new(srv->ctx);
    if(!srv->ssl || BIO_socket_nbio((int)srv->sock, 1) != 1 ||
       SSL_set_fd(srv->ssl, (int)srv->sock) != 1) {
      t3108_server_disconnect(srv);
      return;
    }
    SSL_set_accept_state(srv->ssl);
  }

  /* does the handshake as well */
  nread = SSL_read(srv->ssl, request, (int)sizeof(request));
  if(nread > 0) {
    if(!srv->replied) {
      SSL_write(srv->ssl, reply, (int)(sizeof(reply) - 1));
      SSL_shutdown(srv->ssl);
      srv->replied = TRUE;
    }
  }
  else {
    int err = SSL_get_error(srv->ssl, nread);
    if(err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
      t3108_server_disconnect(srv);
  }
}

static int t3108_count_certinfo(CURL *curl)
{
  struct curl_certinfo *cert_info = NULL;
  int entries = 0;

  if(!curl_easy_getinfo(curl, CURLINFO_CERTINFO, &cert_info) && cert_info) {
    int cert;
    for(cert = 0; cert < cert_info->num_of_certs; cert++) {
      const struct curl_slist *slist = cert_info->certinfo[cert];
      for(; slist; slist = slist->next)
        entries++;
    }
  }
  return entries;
}

/*
 * Do one transfer that fails BIO allocation number 'fail_at' (none if zero).
 * Stores the number of certinfo entries and of BIO allocations seen.
 */
static CURLcode t3108_transfer(struct t3108_server *srv, const char *URL,
                               int fail_at, int *entries, int *bios,
                               CURLcode *transfer_result)
{
  CURL *curl = NULL;
  CURLM *multi = NULL;
  const CURLMsg *msg;
  CURLcode result = CURLE_OK;
  int still_running = 1;
  int msgs_left;

  *entries = 0;
  *bios = 0;
  *transfer_result = CURLE_OK;

  t3108_armed = FALSE;
  t3108_failed = FALSE;
  t3108_bio_count = 0;
  t3108_fail_at = fail_at;

  start_test_timing();

  multi_init(multi);
  easy_init(curl);

  easy_setopt(curl, CURLOPT_URL, URL);
  easy_setopt(curl, CURLOPT_PORT, srv->port);
  easy_setopt(curl, CURLOPT_CERTINFO, 1L);
  easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
  easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  easy_setopt(curl, CURLOPT_SSL_CTX_FUNCTION, t3108_sslctx_cb);

  multi_add_handle(multi, curl);

  while(still_running) {
    CURLMcode mresult;

    t3108_in_client = TRUE;
    mresult = curl_multi_perform(multi, &still_running);
    t3108_in_client = FALSE;
    if(mresult) {
      curl_mfprintf(stderr, "curl_multi_perform() failed, with code %d\n",
                    (int)mresult);
      result = TEST_ERR_MULTI;
      goto test_cleanup;
    }

    abort_on_test_timeout();

    t3108_server_run(srv);
    curlx_wait_ms(1);
  }

  msg = curl_multi_info_read(multi, &msgs_left);
  if(!msg || msg->msg != CURLMSG_DONE) {
    curl_mfprintf(stderr, "transfer ended without a result\n");
    result = TEST_ERR_MAJOR_BAD;
    goto test_cleanup;
  }
  *transfer_result = msg->data.result;
  *bios = t3108_bio_count;
  *entries = t3108_count_certinfo(curl);

test_cleanup:

  t3108_in_client = FALSE;
  t3108_armed = FALSE;
  curl_multi_remove_handle(multi, curl);
  curl_easy_cleanup(curl);
  curl_multi_cleanup(multi);

  return result;
}

static CURLcode test_lib3108(const char *URL)
{
  struct t3108_server srv;
  CURLcode result = CURLE_OK;
  CURLcode transfer_result;
  int base_entries;
  int base_bios;
  int n;

  if(!CRYPTO_set_mem_functions(t3108_malloc, t3108_realloc, t3108_free)) {
    curl_mfprintf(stderr, "CRYPTO_set_mem_functions() failed\n");
    return TEST_ERR_MAJOR_BAD;
  }

  if(curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
    curl_mfprintf(stderr, "curl_global_init() failed\n");
    return TEST_ERR_MAJOR_BAD;
  }

  if(!t3108_server_start(&srv)) {
    curl_mfprintf(stderr, "failed to start the TLS server\n");
    result = TEST_ERR_MAJOR_BAD;
    goto test_cleanup;
  }

  result = t3108_transfer(&srv, URL, 0, &base_entries, &base_bios,
                          &transfer_result);
  if(result)
    goto test_cleanup;
  if(transfer_result || base_entries <= 0 || base_bios <= 0) {
    curl_mfprintf(stderr, "baseline transfer returned %d with %d certinfo "
                  "entries and %d BIO allocations\n",
                  (int)transfer_result, base_entries, base_bios);
    result = TEST_ERR_MAJOR_BAD;
    goto test_cleanup;
  }

  for(n = 1; n <= base_bios; n++) {
    int entries;
    int bios;

    result = t3108_transfer(&srv, URL, n, &entries, &bios, &transfer_result);
    if(result)
      goto test_cleanup;

    if(t3108_failed && !transfer_result && entries < base_entries) {
      curl_mfprintf(stderr, "failed BIO allocation %d returned CURLE_OK "
                    "with %d of %d certinfo entries\n",
                    n, entries, base_entries);
      result = TEST_ERR_FAILURE;
      goto test_cleanup;
    }
  }

test_cleanup:

  t3108_server_stop(&srv);
  curl_global_cleanup();

  return result;
}
#else
/* without OpenSSL memory hooks this does nothing */
static CURLcode test_lib3108(const char *URL)
{
  (void)URL;
  return CURLE_OK;
}
#endif
