/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "ngtcp2_pkt.h"

#include <assert.h>

#include "ngtcp2_conv.h"

void ngtcp2_pkt_hd_init(ngtcp2_pkt_hd *hd, uint8_t flags, uint8_t type,
                        uint64_t conn_id, uint32_t pkt_num, uint32_t version) {
  hd->flags = flags;
  hd->type = type;
  hd->conn_id = conn_id;
  hd->pkt_num = pkt_num;
  hd->version = version;
}

ssize_t ngtcp2_pkt_decode_hd(ngtcp2_pkt_hd *dest, const uint8_t *pkt,
                             size_t pktlen) {
  if (pktlen == 0) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  if (pkt[0] & NGTCP2_HEADER_FORM_MASK) {
    return ngtcp2_pkt_decode_hd_long(dest, pkt, pktlen);
  }

  return ngtcp2_pkt_decode_hd_short(dest, pkt, pktlen);
}

ssize_t ngtcp2_pkt_decode_hd_long(ngtcp2_pkt_hd *dest, const uint8_t *pkt,
                                  size_t pktlen) {
  uint8_t type;

  if (pktlen < NGTCP2_LONG_HEADERLEN) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  if ((pkt[0] & NGTCP2_HEADER_FORM_MASK) == 0) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  type = pkt[0] & NGTCP2_LONG_TYPE_MASK;
  switch (type) {
  case NGTCP2_PKT_VERSION_NEGOTIATION:
  case NGTCP2_PKT_CLIENT_INITIAL:
  case NGTCP2_PKT_SERVER_STATELESS_RETRY:
  case NGTCP2_PKT_SERVER_CLEARTEXT:
  case NGTCP2_PKT_CLIENT_CLEARTEXT:
  case NGTCP2_PKT_0RTT_PROTECTED:
  case NGTCP2_PKT_1RTT_PROTECTED_K0:
  case NGTCP2_PKT_1RTT_PROTECTED_K1:
  case NGTCP2_PKT_PUBLIC_RESET:
    break;
  default:
    return NGTCP2_ERR_UNKNOWN_PKT_TYPE;
  }

  dest->flags = NGTCP2_PKT_FLAG_LONG_FORM;
  dest->type = type;
  dest->conn_id = ngtcp2_get_uint64(&pkt[1]);
  dest->pkt_num = ngtcp2_get_uint32(&pkt[9]);
  dest->version = ngtcp2_get_uint32(&pkt[13]);

  return NGTCP2_LONG_HEADERLEN;
}

ssize_t ngtcp2_pkt_decode_hd_short(ngtcp2_pkt_hd *dest, const uint8_t *pkt,
                                   size_t pktlen) {
  uint8_t flags = 0;
  uint8_t type;
  size_t len = 1;
  const uint8_t *p = pkt;

  if (pktlen < 1) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  if (pkt[0] & NGTCP2_HEADER_FORM_MASK) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  if (pkt[0] & NGTCP2_CONN_ID_MASK) {
    flags |= NGTCP2_PKT_FLAG_CONN_ID;
    len += 8;
  }
  if (pkt[0] & NGTCP2_KEY_PHASE_MASK) {
    flags |= NGTCP2_PKT_FLAG_KEY_PHASE;
  }

  type = pkt[0] & NGTCP2_SHORT_TYPE_MASK;
  switch (type) {
  case NGTCP2_PKT_01:
    ++len;
    break;
  case NGTCP2_PKT_02:
    len += 2;
    break;
  case NGTCP2_PKT_03:
    len += 4;
    break;
  default:
    return NGTCP2_ERR_UNKNOWN_PKT_TYPE;
  }

  if (pktlen < len) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  ++p;

  if (flags & NGTCP2_PKT_FLAG_CONN_ID) {
    dest->conn_id = ngtcp2_get_uint64(p);
    p += 8;
  } else {
    dest->conn_id = 0;
  }

  switch (type) {
  case NGTCP2_PKT_01:
    dest->pkt_num = *p;
    break;
  case NGTCP2_PKT_02:
    dest->pkt_num = ngtcp2_get_uint16(p);
    break;
  case NGTCP2_PKT_03:
    dest->pkt_num = ngtcp2_get_uint32(p);
    break;
  }

  dest->flags = flags;
  dest->version = 0;

  return (ssize_t)len;
}

ssize_t ngtcp2_pkt_encode_hd_long(uint8_t *out, size_t outlen,
                                  const ngtcp2_pkt_hd *hd) {
  uint8_t *p;

  if (outlen < NGTCP2_LONG_HEADERLEN) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  p = out;

  *p++ = NGTCP2_HEADER_FORM_MASK | hd->type;
  p = ngtcp2_put_uint64be(p, hd->conn_id);
  p = ngtcp2_put_uint32be(p, hd->pkt_num);
  p = ngtcp2_put_uint32be(p, hd->version);

  assert(p - out == NGTCP2_LONG_HEADERLEN);

  return NGTCP2_LONG_HEADERLEN;
}

ssize_t ngtcp2_pkt_encode_hd_short(uint8_t *out, size_t outlen,
                                   const ngtcp2_pkt_hd *hd) {
  uint8_t *p;
  size_t len = 1;
  int need_conn_id = 0;

  if (hd->flags & NGTCP2_PKT_FLAG_CONN_ID) {
    need_conn_id = 1;
    len += 8;
  }

  switch (hd->type) {
  case NGTCP2_PKT_01:
    ++len;
    break;
  case NGTCP2_PKT_02:
    len += 2;
    break;
  case NGTCP2_PKT_03:
    len += 4;
    break;
  default:
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  if (outlen < len) {
    return NGTCP2_ERR_INVALID_ARGUMENT;
  }

  p = out;

  *p = hd->type;
  if (need_conn_id) {
    *p |= NGTCP2_CONN_ID_MASK;
  }
  if (hd->flags & NGTCP2_PKT_FLAG_KEY_PHASE) {
    *p |= NGTCP2_KEY_PHASE_MASK;
  }

  ++p;

  if (need_conn_id) {
    p = ngtcp2_put_uint64be(p, hd->conn_id);
  }

  switch (hd->type) {
  case NGTCP2_PKT_01:
    *p++ = (uint8_t)hd->pkt_num;
    break;
  case NGTCP2_PKT_02:
    p = ngtcp2_put_uint16be(p, (uint16_t)hd->pkt_num);
    break;
  case NGTCP2_PKT_03:
    p = ngtcp2_put_uint32be(p, hd->pkt_num);
    break;
  default:
    assert(0);
  }

  assert((size_t)(p - out) == len);

  return p - out;
}