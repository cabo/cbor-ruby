/*
 * CBOR for Ruby
 *
 * Copyright (C) 2013 Carsten Bormann
 *
 *    Licensed under the Apache License, Version 2.0 (the "License").
 *
 * Based on:
 ***********/
/*
 * MessagePack for Ruby
 *
 * Copyright (C) 2008-2013 Sadayuki Furuhashi
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
#ifndef MSGPACK_RUBY_PACKER_H__
#define MSGPACK_RUBY_PACKER_H__

#include "buffer.h"

#ifndef MSGPACK_PACKER_IO_FLUSH_THRESHOLD_TO_WRITE_STRING_BODY
#define MSGPACK_PACKER_IO_FLUSH_THRESHOLD_TO_WRITE_STRING_BODY (1024)
#endif

struct msgpack_packer_t;
typedef struct msgpack_packer_t msgpack_packer_t;

struct msgpack_packer_t {
    msgpack_buffer_t buffer;

    VALUE io;
    ID io_write_all_method;

    ID to_msgpack_method;
    VALUE to_msgpack_arg;

    VALUE buffer_ref;
};

#define PACKER_BUFFER_(pk) (&(pk)->buffer)

void msgpack_packer_static_init();

void msgpack_packer_static_destroy();

void msgpack_packer_init(msgpack_packer_t* pk);

void msgpack_packer_destroy(msgpack_packer_t* pk);

void msgpack_packer_mark(msgpack_packer_t* pk);

static inline void msgpack_packer_set_to_msgpack_method(msgpack_packer_t* pk,
        ID to_msgpack_method, VALUE to_msgpack_arg)
{
    pk->to_msgpack_method = to_msgpack_method;
    pk->to_msgpack_arg = to_msgpack_arg;
}

static inline void msgpack_packer_set_io(msgpack_packer_t* pk, VALUE io, ID io_write_all_method)
{
    pk->io = io;
    pk->io_write_all_method = io_write_all_method;
}

void msgpack_packer_reset(msgpack_packer_t* pk);


static inline void cbor_encoder_write_head(msgpack_packer_t* pk, unsigned int ib, uint64_t n)
{
    if (n < 24) {
        msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), 1);
        msgpack_buffer_write_1(PACKER_BUFFER_(pk), ib + (int)n);
    } else if (n < 256) {
        msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), 3);
        msgpack_buffer_write_2(PACKER_BUFFER_(pk), ib + 24, n);
    } else if (n < 65536) {
        msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), 3);
        uint16_t be = _msgpack_be16(n);
        msgpack_buffer_write_byte_and_data(PACKER_BUFFER_(pk), ib + 25, (const void*)&be, 2);
    } else if (n < 0x100000000LU) {
        msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), 5);
        uint32_t be = _msgpack_be32(n);
        msgpack_buffer_write_byte_and_data(PACKER_BUFFER_(pk), ib + 26, (const void*)&be, 4);
    } else {
        msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), 9);
        uint64_t be = _msgpack_be64(n);
        msgpack_buffer_write_byte_and_data(PACKER_BUFFER_(pk), ib + 27, (const void*)&be, 8);
    }
}

static inline void msgpack_packer_write_nil(msgpack_packer_t* pk)
{
    msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), 1);
    msgpack_buffer_write_1(PACKER_BUFFER_(pk), IB_NIL);
}

static inline void msgpack_packer_write_true(msgpack_packer_t* pk)
{
    msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), 1);
    msgpack_buffer_write_1(PACKER_BUFFER_(pk), IB_TRUE);
}

static inline void msgpack_packer_write_false(msgpack_packer_t* pk)
{
    msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), 1);
    msgpack_buffer_write_1(PACKER_BUFFER_(pk), IB_FALSE);
}

static inline void _msgpack_packer_write_long_long64(msgpack_packer_t* pk, long long v)
{
  uint64_t ui = v >> 63;    // extend sign to whole length
  int mt = ui & IB_NEGFLAG;          // extract major type
  ui ^= v;                 // complement negatives
  cbor_encoder_write_head(pk, mt, ui);
}

static inline void msgpack_packer_write_long(msgpack_packer_t* pk, long v)
{
  _msgpack_packer_write_long_long64(pk, v);
}

static inline void msgpack_packer_write_u64(msgpack_packer_t* pk, uint64_t v)
{
  cbor_encoder_write_head(pk, IB_UNSIGNED, v);
}

static inline void msgpack_packer_write_double(msgpack_packer_t* pk, double v)
{
  float fv = v;
  if (fv == v) {                /* 32 bits is enough and we aren't NaN */
    union {
        float f;
        uint32_t u32;
        char mem[4];
    } castbuf = { fv };
    int b32 = castbuf.u32;
    if ((b32 & 0x1FFF) == 0) { /* worth trying half */
      int s16 = (b32 >> 16) & 0x8000;
      int exp = (b32 >> 23) & 0xff;
      int mant = b32 & 0x7fffff;
      if (exp == 0 && mant == 0)
        ;              /* 0.0, -0.0 */
      else if (exp >= 113 && exp <= 142) /* normalized */
        s16 += ((exp - 112) << 10) + (mant >> 13);
      else if (exp >= 103 && exp < 113) { /* denorm, exp16 = 0 */
        if (mant & ((1 << (126 - exp)) - 1))
          goto float32;         /* loss of precision */
        s16 += ((mant + 0x800000) >> (126 - exp));
      } else if (exp == 255 && mant == 0) { /* Inf */
        s16 += 0x7c00;
      } else
        goto float32;           /* loss of range */
      msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), 3);
      uint16_t be = _msgpack_be16(s16);
      msgpack_buffer_write_byte_and_data(PACKER_BUFFER_(pk), IB_FLOAT2, (const void*)&be, 2);
      return;
    }
  float32:
    msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), 5);
    castbuf.u32 = _msgpack_be_float(castbuf.u32);
    msgpack_buffer_write_byte_and_data(PACKER_BUFFER_(pk), IB_FLOAT4, castbuf.mem, 4);
  } else if (v != v) {          /* NaN */
    cbor_encoder_write_head(pk, 0xe0, 0x7e00);
  } else {
    msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), 9);
    union {
        double d;
        uint64_t u64;
        char mem[8];
    } castbuf = { v };
    castbuf.u64 = _msgpack_be_double(castbuf.u64);
    msgpack_buffer_write_byte_and_data(PACKER_BUFFER_(pk), IB_FLOAT8, castbuf.mem, 8);
  }
}

static inline void msgpack_packer_write_array_header(msgpack_packer_t* pk, uint64_t n)
{
  cbor_encoder_write_head(pk, IB_ARRAY, n);
}

static inline void msgpack_packer_write_map_header(msgpack_packer_t* pk, uint64_t n)
{
  cbor_encoder_write_head(pk, IB_MAP, n);
}


void _msgpack_packer_write_string_to_io(msgpack_packer_t* pk, VALUE string);

static inline void msgpack_packer_write_string_value(msgpack_packer_t* pk, VALUE v)
{
  int mt = IB_TEXT;                /* text string */
#ifdef COMPAT_HAVE_ENCODING
  int enc = ENCODING_GET(v);
  if (enc == s_enc_ascii8bit) {
    mt = IB_BYTES;
  } else if (enc != s_enc_utf8 && enc != s_enc_usascii) {
    if(!ENC_CODERANGE_ASCIIONLY(v)) {
      v = rb_str_encode(v, s_enc_utf8_value, 0, Qnil);
    }
  }
#endif
  cbor_encoder_write_head(pk, mt, RSTRING_LEN(v));
  msgpack_buffer_append_string(PACKER_BUFFER_(pk), v);
}

static inline void msgpack_packer_write_symbol_value(msgpack_packer_t* pk, VALUE v)
{
#ifdef HAVE_RB_SYM2STR
    /* rb_sym2str is added since MRI 2.2.0 */
    msgpack_packer_write_string_value(pk, rb_sym2str(v));
#else
    const char* name = rb_id2name(SYM2ID(v));
    /* actual return type of strlen is size_t */
    unsigned long len = strlen(name);
    if(len > 0xffffffffUL) {
        // TODO rb_eArgError?
        rb_raise(rb_eArgError, "size of symbol is too long to pack: %lu bytes should be <= %lu", len, 0xffffffffUL);
    }
    cbor_encoder_write_head(pk, IB_TEXT, len);
    msgpack_buffer_append(PACKER_BUFFER_(pk), name, len);
#endif
}

static inline void msgpack_packer_write_fixnum_value(msgpack_packer_t* pk, VALUE v)
{
#ifdef JRUBY
    msgpack_packer_write_long(pk, FIXNUM_P(v) ? FIX2LONG(v) : rb_num2ll(v));
#else
    msgpack_packer_write_long(pk, FIX2LONG(v));
#endif
}

static inline void msgpack_packer_write_bignum_value(msgpack_packer_t* pk, VALUE v)
{
  long len;
  int ib = IB_UNSIGNED;
  if (!RBIGNUM_POSITIVE_P(v)) {
    v = rb_funcall(v, rb_intern("~"), 0);  /* should be rb_big_neg(), but that is static. */
    ib = IB_NEGATIVE;
  }


#ifdef HAVE_RB_INTEGER_UNPACK
  len = rb_absint_size(v, NULL);

  if (len > SIZEOF_LONG_LONG) {                  /* i.e., need real bignum */
    msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), 1);
    msgpack_buffer_write_1(PACKER_BUFFER_(pk), IB_BIGNUM + IB_NEGFLAG_AS_BIT(ib));
    cbor_encoder_write_head(pk, IB_BYTES, len);
    msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), len);

    char buf[len];              /* XXX */
    if (rb_integer_pack(v, buf, len, 1, 0, INTEGER_PACK_BIG_ENDIAN) != 1)
      rb_raise(rb_eRangeError, "cbor rb_integer_pack() error");

    msgpack_buffer_append(PACKER_BUFFER_(pk), buf, len);

#else

  len = RBIGNUM_LEN(v);
  if (len > SIZEOF_LONG_LONG/SIZEOF_BDIGITS) {
    msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), 1);
    msgpack_buffer_write_1(PACKER_BUFFER_(pk), IB_BIGNUM + IB_NEGFLAG_AS_BIT(ib));
    {
#ifndef CANT_DO_BIGNUMS_FAST_ON_THIS_PLATFORM
    BDIGIT *dp = RBIGNUM_DIGITS(v);
    BDIGIT *de = dp + len;
    int nbyte = (len - 1) * SIZEOF_BDIGITS;
    int nbmsdig = 0;
    BDIGIT msdig;
    int i;
    if ((msdig = de[-1]) == 0)  /* todo: check whether that occurs */
      rb_raise(rb_eRangeError, "cbor writing unnormalized bignum");
    while (msdig) {     /* get number of significant bytes in msdig */
      nbmsdig++;
      msdig >>= 8;
    }
    nbyte += nbmsdig;
    cbor_encoder_write_head(pk, IB_BYTES, nbyte);
    msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), nbyte);
    /* first digit: */
    msdig = de[-1];
    while (nbmsdig) {
      --nbmsdig;
      msgpack_buffer_write_1(PACKER_BUFFER_(pk), msdig >> (nbmsdig << 3));
    }
    /* rest of the digits: */
    for (i = 1; i < len; ++i) {
      BDIGIT be = NTOHBDIGIT(de[-1-i]);
      msgpack_buffer_append(PACKER_BUFFER_(pk), (const void*)&be, SIZEOF_BDIGITS);
    }
#else
    /* This is a slow workaround only... But a working one.*/
    size_t nbyte;
    VALUE hexval = rb_funcall(v, rb_intern("to_s"), 1, INT2FIX(16));
    char *hp;
    int i;
    if (RSTRING_LEN(hexval) & 1)
      rb_funcall(hexval, rb_intern("[]="), 3, INT2FIX(0), INT2FIX(0), rb_str_new("0", 1));
    nbyte = RSTRING_LEN(hexval) >> 1;
    hp = RSTRING_PTR(hexval);
    cbor_encoder_write_head(pk, IB_BYTES, nbyte);
    msgpack_buffer_ensure_writable(PACKER_BUFFER_(pk), nbyte);
    for (i = 0; i < nbyte; i++) {
      int c;
      sscanf(hp, "%2x", &c);
      hp += 2;
      msgpack_buffer_write_1(PACKER_BUFFER_(pk), c);
    }
#endif
    }
#endif
  } else {
    cbor_encoder_write_head(pk, ib, rb_big2ull(v));
  }

#ifdef RB_GC_GUARD
    RB_GC_GUARD(v);
#endif
}

static inline void msgpack_packer_write_float_value(msgpack_packer_t* pk, VALUE v)
{
    msgpack_packer_write_double(pk, rb_num2dbl(v));
}

static inline void msgpack_packer_write_simple_value(msgpack_packer_t* pk, VALUE v)
{
  cbor_encoder_write_head(pk, IB_PRIM, FIX2LONG(rb_struct_aref(v, INT2FIX(0))));
}

void msgpack_packer_write_array_value(msgpack_packer_t* pk, VALUE v);

void msgpack_packer_write_hash_value(msgpack_packer_t* pk, VALUE v);

void msgpack_packer_write_value(msgpack_packer_t* pk, VALUE v);

static inline void msgpack_packer_write_tagged_value(msgpack_packer_t* pk, VALUE v)
{
  cbor_encoder_write_head(pk, IB_TAG, rb_num2ulong(rb_struct_aref(v, INT2FIX(0))));
  msgpack_packer_write_value(pk, rb_struct_aref(v, INT2FIX(1)));
}

static inline void msgpack_packer_write_processed_value(msgpack_packer_t* pk, VALUE v, ID method, int tag)
{
  cbor_encoder_write_head(pk, IB_TAG, tag);
  msgpack_packer_write_value(pk, rb_funcall(v, method, 0));
}

#endif

