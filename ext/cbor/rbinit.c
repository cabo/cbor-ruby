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

#include "buffer_class.h"
#include "packer_class.h"
#include "unpacker_class.h"
#include "core_ext.h"


VALUE rb_cCBOR_Tagged;
VALUE rb_cCBOR_Simple;

void Init_cbor(void)
{
    VALUE mMessagePack = rb_define_module("CBOR");

    rb_cCBOR_Tagged = rb_struct_define(NULL, "tag", "value", NULL);
    rb_define_const(mMessagePack, "Tagged", rb_cCBOR_Tagged);
    rb_cCBOR_Simple = rb_struct_define(NULL, "value", NULL);
    rb_define_const(mMessagePack, "Simple", rb_cCBOR_Simple);

    MessagePack_Buffer_module_init(mMessagePack);
    MessagePack_Packer_module_init(mMessagePack);
    MessagePack_Unpacker_module_init(mMessagePack);
    MessagePack_core_ext_module_init();
}

