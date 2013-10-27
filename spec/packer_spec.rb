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
end

