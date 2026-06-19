# encoding: ascii-8bit
require 'spec_helper'

require 'stringio'
if defined?(Encoding)
  Encoding.default_external = 'ASCII-8BIT'
end

describe Packer do
  let :packer do
    Packer.new
  end

  it 'initialize' do
    Packer.new
    Packer.new(nil)
    Packer.new(StringIO.new)
    Packer.new({})
    Packer.new(StringIO.new, {})
  end

  #it 'Packer' do
  #  Packer(packer).object_id.should == packer.object_id
  #  Packer(nil).class.should == Packer
  #  Packer('').class.should == Packer
  #  Packer('initbuf').to_s.should == 'initbuf'
  #end

  it 'write' do
    packer.write([])
    packer.to_s.should == "\x80"
  end

  it 'write_nil' do
    packer.write_nil
    packer.to_s.should == "\xf6"
  end

  it 'write_array_header 0' do
    packer.write_array_header(0)
    packer.to_s.should == "\x80"
  end

  it 'write_array_header 1' do
    packer.write_array_header(1)
    packer.to_s.should == "\x81"
  end

  it 'write_map_header 0' do
    packer.write_map_header(0)
    packer.to_s.should == "\xa0"
  end

  it 'write_map_header 1' do
    packer.write_map_header(1)
    packer.to_s.should == "\xa1"
  end

  it 'flush' do
    io = StringIO.new
    pk = Packer.new(io)
    pk.write_nil
    pk.flush
    pk.to_s.should == ''
    io.string.should == "\xf6"
  end

  it 'buffer' do
    o1 = packer.buffer.object_id
    packer.buffer << 'frsyuki'
    packer.buffer.to_s.should == 'frsyuki'
    packer.buffer.object_id.should == o1
  end

  it 'to_cbor returns String' do
    nil.to_cbor.class.should == String
    true.to_cbor.class.should == String
    false.to_cbor.class.should == String
    1.to_cbor.class.should == String
    1.0.to_cbor.class.should == String
    "".to_cbor.class.should == String
    Hash.new.to_cbor.class.should == String
    Array.new.to_cbor.class.should == String
  end

  class CustomPack01
    def to_cbor(pk=nil)
      return MessagePack.pack(self, pk) unless pk.class == MessagePack::Packer
      pk.write_array_header(2)
      pk.write(1)
      pk.write(2)
      return pk
    end
  end

  class CustomPack02
    def to_cbor(pk=nil)
      [1,2].to_cbor(pk)
    end
  end

  it 'calls custom to_cbor method' do
    MessagePack.pack(CustomPack01.new).should == [1,2].to_cbor
    MessagePack.pack(CustomPack02.new).should == [1,2].to_cbor
    CustomPack01.new.to_cbor.should == [1,2].to_cbor
    CustomPack02.new.to_cbor.should == [1,2].to_cbor
  end

  it 'calls custom to_cbor method with io' do
    s01 = StringIO.new
    MessagePack.pack(CustomPack01.new, s01)
    s01.string.should == [1,2].to_cbor

    s02 = StringIO.new
    MessagePack.pack(CustomPack02.new, s02)
    s02.string.should == [1,2].to_cbor

    s03 = StringIO.new
    CustomPack01.new.to_cbor(s03)
    s03.string.should == [1,2].to_cbor

    s04 = StringIO.new
    CustomPack02.new.to_cbor(s04)
    s04.string.should == [1,2].to_cbor
  end

  # These examples exercise every head-size branch of cbor_encoder_write_head
  # (ext/cbor/packer.h) that calls msgpack_buffer_write_byte_and_data.
  #
  # Note on buffer safety: an earlier version of this PR proposed adding
  #   assert(length + 1 <= msgpack_buffer_writable_size(b))
  # inside msgpack_buffer_write_byte_and_data (ext/cbor/buffer.h).
  # That assertion was dropped on @cabo's review: msgpack_buffer_ensure_writable
  # is called on the line immediately before *every* msgpack_buffer_write_byte_and_data
  # call site, and it expands the buffer to at least `require' bytes when needed
  # (buffer.h:232-237). The assertion would therefore be tautologically true — it
  # can never fire — and documents no new invariant worth the noise.
  #
  # The examples below provide round-trip regression coverage confirming that
  # every branch encodes and decodes correctly.
  describe 'head-size branches into write_byte_and_data' do
    it 'encodes uint in 16-bit head range (256..65535) and round-trips' do
      [256, 1000, 65535].each do |n|
        bytes = CBOR.encode(n)
        bytes.bytes[0].should == 0x19
        bytes.bytesize.should == 3
        CBOR.decode(bytes).should == n
      end
    end

    it 'encodes uint in 32-bit head range (65536..2**32-1) and round-trips' do
      [65536, 1_000_000, (2**32) - 1].each do |n|
        bytes = CBOR.encode(n)
        bytes.bytes[0].should == 0x1a
        bytes.bytesize.should == 5
        CBOR.decode(bytes).should == n
      end
    end

    it 'encodes uint in 64-bit head range (>= 2**32) and round-trips' do
      [2**32, 2**40, (2**64) - 1].each do |n|
        bytes = CBOR.encode(n)
        bytes.bytes[0].should == 0x1b
        bytes.bytesize.should == 9
        CBOR.decode(bytes).should == n
      end
    end

    it 'encodes negative ints across head-size branches and round-trips' do
      # negative encoding uses major type 1 (0x20 base) with same AI ladder
      [-257, -65537, -(2**32) - 1].each do |n|
        CBOR.decode(CBOR.encode(n)).should == n
      end
    end

    it 'encodes byte/text strings whose length hits the 16-bit head branch' do
      s = "x" * 1000
      bytes = CBOR.encode(s)
      bytes.bytes[0].should == (0x60 + 25) # IB_TEXT + AI_2 = 0x79
      CBOR.decode(bytes).should == s

      b = ("\x00".b * 1000)
      enc = CBOR.encode(b)
      enc.bytes[0].should == (0x40 + 25) # IB_BYTES + AI_2 = 0x59
      CBOR.decode(enc).should == b
    end

    it 'encodes byte string whose length hits the 32-bit head branch' do
      s = "x" * 70_000
      bytes = CBOR.encode(s)
      bytes.bytes[0].should == (0x60 + 26) # IB_TEXT + AI_4 = 0x7a
      bytes.bytesize.should == 5 + s.bytesize
      CBOR.decode(bytes).should == s
    end

    it 'encodes array/map headers across head-size branches' do
      packer.write_array_header(500)
      packer.to_s.bytes[0].should == (0x80 + 25) # IB_ARRAY + AI_2 = 0x99

      pk2 = Packer.new
      pk2.write_array_header(70_000)
      pk2.to_s.bytes[0].should == (0x80 + 26) # IB_ARRAY + AI_4 = 0x9a

      pk3 = Packer.new
      pk3.write_map_header(500)
      pk3.to_s.bytes[0].should == (0xa0 + 25) # IB_MAP + AI_2 = 0xb9
    end

    it 'encodes floats via half/single/double paths and round-trips' do
      # half (3-byte): exactly-representable small value
      half_bytes = CBOR.encode(1.5)
      half_bytes.bytes[0].should == 0xf9
      half_bytes.bytesize.should == 3
      CBOR.decode(half_bytes).should == 1.5

      # single (5-byte): needs float32 precision but not float64
      single_bytes = CBOR.encode(100000.5)
      single_bytes.bytes[0].should == 0xfa
      single_bytes.bytesize.should == 5
      CBOR.decode(single_bytes).should == 100000.5

      # double (9-byte): value not representable in float32
      double_bytes = CBOR.encode(1.1)
      double_bytes.bytes[0].should == 0xfb
      double_bytes.bytesize.should == 9
      CBOR.decode(double_bytes).should == 1.1
    end
  end
end

