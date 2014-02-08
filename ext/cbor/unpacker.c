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

#include "unpacker.h"
#include "rmem.h"
#include <math.h>               /* for ldexp */

#if !defined(DISABLE_RMEM) && !defined(DISABLE_UNPACKER_STACK_RMEM) && \
        MSGPACK_UNPACKER_STACK_CAPACITY * MSGPACK_UNPACKER_STACK_SIZE <= MSGPACK_RMEM_PAGE_SIZE
#define UNPACKER_STACK_RMEM
#endif

#ifdef UNPACKER_STACK_RMEM
static msgpack_rmem_t s_stack_rmem;
#endif

void msgpack_unpacker_static_init()
{
#ifdef UNPACKER_STACK_RMEM
    msgpack_rmem_init(&s_stack_rmem);
#endif

#ifdef COMPAT_HAVE_ENCODING
    s_enc_utf8 = rb_utf8_encindex();
#endif
}

void msgpack_unpacker_static_destroy()
{
#ifdef UNPACKER_STACK_RMEM
    msgpack_rmem_destroy(&s_stack_rmem);
#endif
}

#define HEAD_BYTE_REQUIRED IB_UNUSED

void msgpack_unpacker_init(msgpack_unpacker_t* uk)
{
    memset(uk, 0, sizeof(msgpack_unpacker_t));

    msgpack_buffer_init(UNPACKER_BUFFER_(uk));

    uk->head_byte = HEAD_BYTE_REQUIRED;

    uk->last_object = Qnil;
    uk->reading_raw = Qnil;

#ifdef UNPACKER_STACK_RMEM
    uk->stack = msgpack_rmem_alloc(&s_stack_rmem);
    /*memset(uk->stack, 0, MSGPACK_UNPACKER_STACK_CAPACITY);*/
#else
    /*uk->stack = calloc(MSGPACK_UNPACKER_STACK_CAPACITY, sizeof(msgpack_unpacker_stack_t));*/
    uk->stack = malloc(MSGPACK_UNPACKER_STACK_CAPACITY * sizeof(msgpack_unpacker_stack_t));
#endif
    uk->stack_capacity = MSGPACK_UNPACKER_STACK_CAPACITY;
}

void msgpack_unpacker_destroy(msgpack_unpacker_t* uk)
{
#ifdef UNPACKER_STACK_RMEM
    msgpack_rmem_free(&s_stack_rmem, uk->stack);
#else
    free(uk->stack);
#endif

    msgpack_buffer_destroy(UNPACKER_BUFFER_(uk));
}

void msgpack_unpacker_mark(msgpack_unpacker_t* uk)
{
    rb_gc_mark(uk->last_object);
    rb_gc_mark(uk->reading_raw);

    msgpack_unpacker_stack_t* s = uk->stack;
    msgpack_unpacker_stack_t* send = uk->stack + uk->stack_depth;
    for(; s < send; s++) {
        rb_gc_mark(s->object);
        rb_gc_mark(s->key);
    }

    /* See MessagePack_Buffer_wrap */
    /* msgpack_buffer_mark(UNPACKER_BUFFER_(uk)); */
    rb_gc_mark(uk->buffer_ref);
}

void msgpack_unpacker_reset(msgpack_unpacker_t* uk)
{
    msgpack_buffer_clear(UNPACKER_BUFFER_(uk));

    uk->head_byte = HEAD_BYTE_REQUIRED;

    /*memset(uk->stack, 0, sizeof(msgpack_unpacker_t) * uk->stack_depth);*/
    uk->stack_depth = 0;

    uk->last_object = Qnil;
    uk->reading_raw = Qnil;
    uk->reading_raw_remaining = 0;
}


/* head byte functions */
static int read_head_byte(msgpack_unpacker_t* uk)
{
    int r = msgpack_buffer_read_1(UNPACKER_BUFFER_(uk));
    if(r == -1) {
        return PRIMITIVE_EOF;
    }
    return uk->head_byte = r;
}

static inline int get_head_byte(msgpack_unpacker_t* uk)
{
    int b = uk->head_byte;
    if(b == HEAD_BYTE_REQUIRED) {
        b = read_head_byte(uk);
    }
    return b;
}

static inline void reset_head_byte(msgpack_unpacker_t* uk)
{
    uk->head_byte = HEAD_BYTE_REQUIRED;
}

static inline int object_complete(msgpack_unpacker_t* uk, VALUE object)
{
    uk->last_object = object;
    reset_head_byte(uk);
    return PRIMITIVE_OBJECT_COMPLETE;
}

static inline VALUE object_string_encoding_set(VALUE str, int textflag) {
#ifdef COMPAT_HAVE_ENCODING
  //str_modifiable(str);
  ENCODING_SET(str, textflag ? s_enc_utf8: s_enc_ascii8bit);
#endif
  return str;
}

static inline int object_complete_string(msgpack_unpacker_t* uk, VALUE str, int textflag)
{
  return object_complete(uk, object_string_encoding_set(str, textflag));
}

/* stack funcs */
static inline msgpack_unpacker_stack_t* _msgpack_unpacker_stack_top(msgpack_unpacker_t* uk)
{
    return &uk->stack[uk->stack_depth-1];
}

static inline int _msgpack_unpacker_stack_push_tag(msgpack_unpacker_t* uk, enum stack_type_t type, size_t count, VALUE object, uint64_t tag)
{
    reset_head_byte(uk);

    if(uk->stack_capacity - uk->stack_depth <= 0) {
        return PRIMITIVE_STACK_TOO_DEEP;
    }

    msgpack_unpacker_stack_t* next = &uk->stack[uk->stack_depth];
    next->count = count;
    next->type = type;
    next->object = object;
    next->key = Qnil;
    next->tag = tag;

    uk->stack_depth++;
    return PRIMITIVE_CONTAINER_START;
}

static inline int _msgpack_unpacker_stack_push(msgpack_unpacker_t* uk, enum stack_type_t type, size_t count, VALUE object)
{
  return _msgpack_unpacker_stack_push_tag(uk, type, count, object, 0);
}

static inline VALUE msgpack_unpacker_stack_pop(msgpack_unpacker_t* uk)
{
    return --uk->stack_depth;
}

static inline bool msgpack_unpacker_stack_is_empty(msgpack_unpacker_t* uk)
{
    return uk->stack_depth == 0;
}

#ifdef USE_CASE_RANGE

#define SWITCH_RANGE_BEGIN(BYTE)     { switch(BYTE) {
#define SWITCH_RANGE(BYTE, FROM, TO) } case FROM ... TO: {
#define SWITCH_RANGE_DEFAULT         } default: {
#define SWITCH_RANGE_END             } }

#else

#define SWITCH_RANGE_BEGIN(BYTE)     { if(0) {
#define SWITCH_RANGE(BYTE, FROM, TO) } else if(FROM <= (BYTE) && (BYTE) <= TO) {
#define SWITCH_RANGE_DEFAULT         } else {
#define SWITCH_RANGE_END             } }

#endif


#define READ_CAST_BLOCK_OR_RETURN_EOF(cb, uk, n) \
    union msgpack_buffer_cast_block_t* cb = msgpack_buffer_read_cast_block(UNPACKER_BUFFER_(uk), n); \
    if(cb == NULL) { \
        return PRIMITIVE_EOF; \
    }

static inline bool is_reading_map_key(msgpack_unpacker_t* uk)
{
    if(uk->stack_depth > 0) {
        msgpack_unpacker_stack_t* top = _msgpack_unpacker_stack_top(uk);
        if(top->type == STACK_TYPE_MAP_KEY || top->type == STACK_TYPE_MAP_KEY_INDEF) {
            return true;
        }
    }
    return false;
}

static int read_raw_body_cont(msgpack_unpacker_t* uk, int textflag)
{
    size_t length = uk->reading_raw_remaining;

    if(uk->reading_raw == Qnil) {
        uk->reading_raw = rb_str_buf_new(length);
    }

    do {
        size_t n = msgpack_buffer_read_to_string(UNPACKER_BUFFER_(uk), uk->reading_raw, length);
        if(n == 0) {
            return PRIMITIVE_EOF;
        }
        /* update reading_raw_remaining everytime because
         * msgpack_buffer_read_to_string raises IOError */
        uk->reading_raw_remaining = length = length - n;
    } while(length > 0);

    object_complete_string(uk, uk->reading_raw, textflag);
    uk->reading_raw = Qnil;
    return PRIMITIVE_OBJECT_COMPLETE;
}

static inline int read_raw_body_begin(msgpack_unpacker_t* uk, int textflag)
{
    /* assuming uk->reading_raw == Qnil */
    uk->textflag = textflag;

    /* try optimized read */
    size_t length = uk->reading_raw_remaining;
    if(length <= msgpack_buffer_top_readable_size(UNPACKER_BUFFER_(uk))) {
        /* don't use zerocopy for hash keys but get a frozen string directly
         * because rb_hash_aset freezes keys and it causes copying */
        bool will_freeze = is_reading_map_key(uk);
        VALUE string;
        bool as_symbol = will_freeze && textflag && uk->keys_as_symbols;
        string = msgpack_buffer_read_top_as_string(UNPACKER_BUFFER_(uk), length, will_freeze, as_symbol);
        if (as_symbol)
          object_complete(uk, string);
        else {
          object_complete_string(uk, string, textflag);
          if (will_freeze) {
            rb_obj_freeze(string);
          }
        }

        uk->reading_raw_remaining = 0;
        return PRIMITIVE_OBJECT_COMPLETE;
    }

    return read_raw_body_cont(uk, textflag);
}

/* speedier version of ldexp for 11-bit numbers  */
static inline double ldexp11(int mant, int exp) {
#if SOMEBODY_FIXED_LDEXP
  return ldexp(mant, exp);
#else
  union {
    double d;
    uint64_t u;
  } u;
  u.u = ((mant & 0x3ffULL) << 42) | ((exp + 1033ULL) << 52);
  return u.d;
#endif
}

#define READ_VAL(uk, ai, val)  {                \
    int n = 1 << ((ai) & 3);                    \
    READ_CAST_BLOCK_OR_RETURN_EOF(cb, uk, n);   \
    switch ((ai) & 3) {                         \
    case 0: {                                   \
      val = cb->u8;                             \
      break;                                    \
    }                                           \
    case 1: {                                   \
      val = _msgpack_be16(cb->u16);             \
      break;                                    \
    }                                           \
    case 2: {                                   \
      val = _msgpack_be32(cb->u32);             \
      break;                                    \
    }                                           \
    case 3: {                                   \
      val = _msgpack_be64(cb->u64);             \
      break;                                    \
    }                                           \
    }                                           \
  }


/* All the cases with an immediate value in the AI */
#define CASE_IMM(n) case ((n*8)): case ((n*8) + 1): case ((n*8) + 2): case ((n*8) + 3): case ((n*8) + 4): case ((n*8) + 5)
/* The case with additional information */
#define CASE_AI(n) case ((n*8) + 6)
/* The case with the reserved and indefinite ai values */
#define CASE_INDEF(n) case ((n*8) + 7)

static int read_primitive(msgpack_unpacker_t* uk)
{
    if(uk->reading_raw_remaining > 0) {
      return read_raw_body_cont(uk, uk->textflag);
    }

    int ib = get_head_byte(uk);
    if (ib < 0) {
        return ib;
    }

    int ai = IB_AI(ib);
    uint64_t val = ai;

    switch (ib >> 2) {
    CASE_IMM(MT_UNSIGNED): // Positive Fixnum
      return object_complete(uk, INT2FIX(ai));
    CASE_AI(MT_UNSIGNED):
      READ_VAL(uk, ai, val);
      return object_complete(uk, rb_ull2inum(val));
    CASE_IMM(MT_NEGATIVE): // Negative Fixnum
      return object_complete(uk, INT2FIX(~ai));
    CASE_AI(MT_NEGATIVE):
      READ_VAL(uk, ai, val);
      return object_complete(uk,
                             (val & 0x8000000000000000
                              ? rb_funcall(rb_ull2inum(val), rb_intern("~"), 0)
                              : rb_ll2inum(~val)));
    CASE_AI(MT_BYTES): CASE_AI(MT_TEXT):
      READ_VAL(uk, ai, val);
    CASE_IMM(MT_BYTES): // byte string
    CASE_IMM(MT_TEXT): // text string
        if (val == 0) {
          return object_complete_string(uk, rb_str_buf_new(0), ib & IB_TEXTFLAG);
        }
        uk->reading_raw_remaining = val; /* TODO: range checks on val here and below */
        return read_raw_body_begin(uk, ib & IB_TEXTFLAG);
    CASE_AI(MT_ARRAY):
      READ_VAL(uk, ai, val);
    CASE_IMM(MT_ARRAY): // array
        if (val == 0) {
            return object_complete(uk, rb_ary_new());
        }
        return _msgpack_unpacker_stack_push(uk, STACK_TYPE_ARRAY, val, rb_ary_new2(val));
    CASE_AI(MT_MAP):
      READ_VAL(uk, ai, val);
    CASE_IMM(MT_MAP): // map
        if (val == 0) {
            return object_complete(uk, rb_hash_new());
        }
        return _msgpack_unpacker_stack_push(uk, STACK_TYPE_MAP_KEY, val*2, rb_hash_new());
    CASE_AI(MT_TAG):
      READ_VAL(uk, ai, val);
    CASE_IMM(MT_TAG): // tag
      return _msgpack_unpacker_stack_push_tag(uk, STACK_TYPE_TAG, 1, Qnil, val);
    case MT_PRIM*8+5:
        switch(ai) {
        case VAL_NIL:  // nil
            return object_complete(uk, Qnil);
        case VAL_FALSE:  // false
            return object_complete(uk, Qfalse);
        case VAL_TRUE:  // true
            return object_complete(uk, Qtrue);
        }
        /* fall through */
    case MT_PRIM*8: case MT_PRIM*8+1: case MT_PRIM*8+2: case MT_PRIM*8+3: case MT_PRIM*8+4:
      return object_complete(uk, rb_struct_new(rb_cCBOR_Simple, INT2FIX(val)));
    CASE_AI(MT_PRIM):
      READ_VAL(uk, ai, val);
      switch (ai) {
        case AI_2: {  // half
          int exp = (val >> 10) & 0x1f;
          int mant = val & 0x3ff;
          double res;
          if (exp == 0) res = ldexp(mant, -24);
          else if (exp != 31) res = ldexp11(mant + 1024, exp - 25);
          else res = mant == 0 ? INFINITY : NAN;
          return object_complete(uk, rb_float_new(val & 0x8000 ? -res : res));
        }
        case AI_4:  // float
            {
              union {
                uint32_t u32;
                float f;
              } castbuf = { val };
              return object_complete(uk, rb_float_new(castbuf.f));
            }
        case AI_8:  // double
            {
              union {
                uint64_t u64;
                double d;
              } castbuf = { val };
              return object_complete(uk, rb_float_new(castbuf.d));
            }
        default:
          return object_complete(uk, rb_struct_new(rb_cCBOR_Simple, INT2FIX(val)));
      }
    CASE_INDEF(MT_BYTES): CASE_INDEF(MT_TEXT):           /* handle string */
      if (ai == AI_INDEF)
        return _msgpack_unpacker_stack_push(uk, STACK_TYPE_STRING_INDEF, ib & IB_TEXTFLAG,
                                            object_string_encoding_set(rb_str_buf_new(0),
                                                                       ib & IB_TEXTFLAG));
      /* fall through */
    CASE_INDEF(MT_ARRAY):
      if (ai == AI_INDEF)
        return _msgpack_unpacker_stack_push(uk, STACK_TYPE_ARRAY_INDEF, 0, rb_ary_new2(0));
      /* fall through */
    CASE_INDEF(MT_MAP):
      if (ai == AI_INDEF)
        return _msgpack_unpacker_stack_push(uk, STACK_TYPE_MAP_KEY_INDEF, 0, rb_hash_new());
      /* fall through */
    CASE_INDEF(MT_PRIM):
      if (ai == AI_INDEF)
        return PRIMITIVE_BREAK;
      /* fall through */
    }
    return PRIMITIVE_INVALID_BYTE;
}


int msgpack_unpacker_read_container_header(msgpack_unpacker_t* uk, uint64_t* result_size, int ib)
{
    int b = get_head_byte(uk);
    if(b < 0) {
        return b;
    }

    if(ib <= b && b < (ib + AI_1)) {
      *result_size = IB_AI(b);
    } else if((b & ~0x3) == (ib + AI_1)) {
      uint64_t val;
      READ_VAL(uk, b, val);
      *result_size = val;
    } else {
        return PRIMITIVE_UNEXPECTED_TYPE; /* including INDEF! */
    }

    return 0;
}


int msgpack_unpacker_read_array_header(msgpack_unpacker_t* uk, uint64_t* result_size) {
  return msgpack_unpacker_read_container_header(uk, result_size, IB_ARRAY);
}

int msgpack_unpacker_read_map_header(msgpack_unpacker_t* uk, uint64_t* result_size) {
  return msgpack_unpacker_read_container_header(uk, result_size, IB_MAP);
}

static VALUE msgpack_unpacker_process_tag(uint64_t tag, VALUE v) {
  VALUE res = v;
  switch (tag) {
  case TAG_TIME_EPOCH: {
    return rb_funcall(rb_cTime, rb_intern("at"), 1, v);
  }
  case TAG_RE: {
    return rb_funcall(rb_cRegexp, rb_intern("new"), 1, v);
  }
  case TAG_URI: {
    if (!rb_const_defined(rb_cObject, rb_intern("URI")))
      goto unknown_tag;
    return rb_funcall(rb_const_get(rb_cObject, rb_intern("URI")),
                      rb_intern("parse"), 1, v);
  }
  case TAG_BIGNUM_NEG:
  case TAG_BIGNUM:
    /* check object is a byte string */
    if (rb_type(v) != T_STRING)
      break;                    /* XXX: really should error out here */
#ifdef COMPAT_HAVE_ENCODING
    if (ENCODING_GET(v) != s_enc_ascii8bit)
      break;                    /* XXX: really should error out here */
#endif
    {
      char *sp = RSTRING_PTR(v);
      int slen = RSTRING_LEN(v);
      while (slen && *sp == 0) {
        slen--;
        sp++;
      }
#ifndef CANT_DO_BIGNUMS_FAST_ON_THIS_PLATFORM
      int ndig = (slen + SIZEOF_BDIGITS - 1)/SIZEOF_BDIGITS;
      res = rb_big_new(ndig, 1);
    /* compute number RSTRING_PTR(v) */
      if (ndig) {
        BDIGIT *ds = RBIGNUM_DIGITS(res);
        int missbytes = ndig*SIZEOF_BDIGITS - slen; /* 0..SIZEOF_BDIGITS - 1 */
        union {
          char msdig_str[SIZEOF_BDIGITS];
          BDIGIT msdig_bd;
        } u;
        u.msdig_bd = 0;
        memcpy(u.msdig_str + missbytes, sp, SIZEOF_BDIGITS - missbytes);
        ds[ndig-1] = NTOHBDIGIT(u.msdig_bd);
        sp += SIZEOF_BDIGITS - missbytes;
        while (--ndig > 0) {
          memcpy(u.msdig_str, sp, SIZEOF_BDIGITS);
          ds[ndig-1] = NTOHBDIGIT(u.msdig_bd);
          sp += SIZEOF_BDIGITS;
        }
      }
#else
      {
        /* This is a slow workaround only... But a working one. */
        char *hex = ALLOC_N(char, 2 * slen + 1);
        char *p = hex;
        while (slen) {
          sprintf(p, "%02x", *sp++ & 0xFF);
          p += 2;
          slen--;
        }
        *p = 0;
        res = rb_cstr2inum(hex, 16);
        xfree(hex);
      }
#endif
      if (tag == TAG_BIGNUM)    /* non-negative */
#ifndef CANT_DO_BIGNUMS_FAST_ON_THIS_PLATFORM
        return rb_big_norm(res);
#else
        return res;
#endif
      else
        return rb_funcall(res, rb_intern("~"), 0);  /* should be rb_big_neg(), but that is static. */
    }
  }
unknown_tag:
  /* common return for unknown tags */
  return rb_struct_new(rb_cCBOR_Tagged, rb_ull2inum(tag), v);
}

int msgpack_unpacker_read(msgpack_unpacker_t* uk, size_t target_stack_depth)
{
    while(true) {
        int r = read_primitive(uk);
        if(r < 0) {
            return r;
        }
        if(r == PRIMITIVE_CONTAINER_START) {
            continue;
        }
        /* PRIMITIVE_OBJECT_COMPLETE */

        if(msgpack_unpacker_stack_is_empty(uk)) {
            if (r == PRIMITIVE_BREAK)
              return PRIMITIVE_INVALID_BYTE;
            return PRIMITIVE_OBJECT_COMPLETE;
        }

        container_completed:
        {
            msgpack_unpacker_stack_t* top = _msgpack_unpacker_stack_top(uk);
            if (top->type <= STACK_TYPE_MAP_VALUE_INDEF && r == PRIMITIVE_BREAK)
              return PRIMITIVE_INVALID_BYTE;
            switch(top->type) {
            case STACK_TYPE_ARRAY:
                rb_ary_push(top->object, uk->last_object);
                break;
            case STACK_TYPE_MAP_KEY:
                top->key = uk->last_object;
                top->type = STACK_TYPE_MAP_VALUE;
                break;
            case STACK_TYPE_MAP_VALUE:
                rb_hash_aset(top->object, top->key, uk->last_object);
                top->type = STACK_TYPE_MAP_KEY;
                break;
            case STACK_TYPE_TAG:
              object_complete(uk, msgpack_unpacker_process_tag(top->tag, uk->last_object));
              goto done;
              
            case STACK_TYPE_ARRAY_INDEF:
              if (r == PRIMITIVE_BREAK)
                goto complete;
              rb_ary_push(top->object, uk->last_object);
              continue;
            case STACK_TYPE_MAP_KEY_INDEF:
              if (r == PRIMITIVE_BREAK)
                goto complete;
              top->key = uk->last_object;
              top->type = STACK_TYPE_MAP_VALUE_INDEF;
              continue;
            case STACK_TYPE_MAP_VALUE_INDEF:
              rb_hash_aset(top->object, top->key, uk->last_object);
              top->type = STACK_TYPE_MAP_KEY_INDEF;
              continue;
            case STACK_TYPE_STRING_INDEF:
              if (r == PRIMITIVE_BREAK) {
                object_complete_string(uk, top->object, top->count); /* use count as textflag */
                goto done;
              }
              if (!RB_TYPE_P(uk->last_object, T_STRING))
                return PRIMITIVE_INVALID_BYTE;
#ifdef COMPAT_HAVE_ENCODING     /* XXX */
              if (ENCODING_GET(top->object) != ENCODING_GET(uk->last_object))
                return PRIMITIVE_INVALID_BYTE;
#endif
              rb_str_append(top->object, uk->last_object);
              continue;
            }
            size_t count = --top->count;

            if(count == 0) {
            complete:;
              object_complete(uk, top->object);
            done:;
                if(msgpack_unpacker_stack_pop(uk) <= target_stack_depth) {
                    return PRIMITIVE_OBJECT_COMPLETE;
                }
                goto container_completed;
            }
        }
    }
}

int msgpack_unpacker_skip(msgpack_unpacker_t* uk, size_t target_stack_depth)
{
    while(true) {
        int r = read_primitive(uk);
        if(r < 0) {
            return r;
        }
        if(r == PRIMITIVE_CONTAINER_START) {
            continue;
        }
        /* PRIMITIVE_OBJECT_COMPLETE */

        if(msgpack_unpacker_stack_is_empty(uk)) {
            return PRIMITIVE_OBJECT_COMPLETE;
        }

        container_completed:
        {
            msgpack_unpacker_stack_t* top = _msgpack_unpacker_stack_top(uk);

            /* this section optimized out */
            // TODO object_complete still creates objects which should be optimized out
            /* XXX: check INDEF! */

            size_t count = --top->count;

            if(count == 0) {
                object_complete(uk, Qnil);
                if(msgpack_unpacker_stack_pop(uk) <= target_stack_depth) {
                    return PRIMITIVE_OBJECT_COMPLETE;
                }
                goto container_completed;
            }
        }
    }
}

/* dead code, but keep it for comparison purposes */

static enum msgpack_unpacker_object_type msgpack_unpacker_object_types_per_mt[] = {
    TYPE_INTEGER, TYPE_INTEGER,
    TYPE_RAW, TYPE_RAW,         /* XXX */
    TYPE_ARRAY, TYPE_MAP,
    TYPE_INTEGER,               /* XXX */
    TYPE_NIL                    /* see below: */
};

static enum msgpack_unpacker_object_type msgpack_unpacker_object_types_per_ai[] = {
  PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE,
  PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE,
  PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE,
  PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE,
  PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE,
  TYPE_BOOLEAN, TYPE_BOOLEAN, TYPE_NIL, PRIMITIVE_INVALID_BYTE, /* XXX */
  PRIMITIVE_INVALID_BYTE, TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT,
  PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE, PRIMITIVE_INVALID_BYTE,
};

int msgpack_unpacker_peek_next_object_type(msgpack_unpacker_t* uk)
{
    int ib = get_head_byte(uk);
    if (ib < 0) {
        return ib;
    }

    int t = msgpack_unpacker_object_types_per_mt[IB_MT(ib)];
    if (t == TYPE_NIL) {
      t = msgpack_unpacker_object_types_per_ai[IB_AI(ib)];
    }
    return t;
}

int msgpack_unpacker_skip_nil(msgpack_unpacker_t* uk)
{
    int b = get_head_byte(uk);
    if(b < 0) {
        return b;
    }
    if (b == IB_NIL) {
        return 1;
    }
    return 0;
}

