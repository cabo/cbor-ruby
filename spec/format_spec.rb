# encoding: utf-8
require 'spec_helper'

def bignum_to_bytes(bn)
  bn.to_s(16).chars.each_slice(2).map{|a| a.join.to_i(16).chr}.join
end
# => "\x98!sHq#\x98W\x91\x82sY\x87)Hu#E"
# 0x982173487123985791827359872948752345

unless defined?(Float::INFINITY)
  class Float
    INFINITY = 1.0/0.0
    NAN = 0.0/0.0
  end
end

describe MessagePack do
  it "nil" do
    check 1, nil
  end

  it "true" do
    check 1, true
  end

  it "false" do
    check 1, false
  end

  it "zero" do
    check 1, 0
  end

  it "positive fixnum" do
    check 1, 1
    check 2, (1<<6)
    check 2, (1<<7)-1
  end

  it "positive int 8" do
    check 1, -1
    check 2, (1<<7)
    check 2, (1<<8)-1
  end

  it "positive int 16" do
    check 3, (1<<8)
    check 3, (1<<16)-1
    check 3, 1024
    check 4, [1024]
  end

  it "positive int 32" do
    check 5, (1<<16)
    check 5, (1<<32)-1
  end

  it "positive int 64" do
    check 9, (1<<32)
    check 9, (1<<64)-1
  end

  it "negative fixnum" do
    check 1, -1
    check 1, -24
    check 2, -25
  end

  it "negative int 8" do
    check 2, -((1<<5)+1)
    check 2, -(1<<7)
  end

  it "negative int 16" do
    check 2, -((1<<7)+1)
    check 2, -256
    check 3, -(1<<15)
  end

  it "negative int 32" do
    check 3, -((1<<15)+1)
    check 3, -(1<<16)
    check 5, -(1<<31)
    check 5, -(1<<32)
  end

  it "negative int 64" do
    check 5, -((1<<31)+1)
    check 9, -(1<<63)
    check 9, -(1<<64)
  end

  it "half" do
    check 3, 1.0
    check 3, -1.0
    check 3, -2.0
    check 3, 65504.0
    check 3, -65504.0
    check 3, Math.ldexp(1, -14) # ≈ 6.10352 × 10−5 (minimum positive normal)
    check 3, -Math.ldexp(1, -14) # ≈ 6.10352 × 10−5 (maximum negative normal)
    check 3, Math.ldexp(1, -14) - Math.ldexp(1, -24) # ≈ 6.09756 × 10−5 (maximum subnormal)
    check 3, Math.ldexp(1, -24) # ≈ 5.96046 × 10−8 (minimum positive subnormal)
    check 3, -Math.ldexp(1, -24) # ≈ -5.96046 × 10−8 (maximum negative subnormal)
    check 5, Math.ldexp(1, -14) - Math.ldexp(0.5, -24) # check loss of precision
    check 5, Math.ldexp(1.5, -24)                      # loss of precision (subnormal)
    check 3, Math.ldexp(1.5, -23)
    check 5, Math.ldexp(1.75, -23)
    check 3, Float::INFINITY
    check 3, -Float::INFINITY
    # check 3, Float::NAN     # NAN is not equal to itself, to this never checks out...
    raw = Float::NAN.to_cbor.to_s
    raw.length.should == 3
    MessagePack.unpack(raw).nan?.should == true
  end

  it "double" do
    check 9, 0.1
    check 9, -0.1
  end

  it "fixraw" do
    check_raw 1, 0
    check_raw 1, 23
  end

  it "raw 16" do
    check_raw 2, (1<<5)
    check_raw 3, (1<<16)-1
  end

  it "raw 32" do
    check_raw 5, (1<<16)
    #check_raw 5, (1<<32)-1  # memory error
  end

  it "fixarray" do
    check_array 1, 0
    check_array 1, (1<<4)-1
    check_array 1, 23
  end

  it "array 16" do
    check_array 1, (1<<4)
    #check_array 3, (1<<16)-1
  end

  it "array 32" do
    #check_array 5, (1<<16)
    #check_array 5, (1<<32)-1  # memory error
  end

  it "nil" do
    match nil, "\xf6".b
  end

  it "false" do
    match false, "\xf4".b
  end

  it "true" do
    match true, "\xf5".b
  end

  it "0" do
    match 0, "\x00".b
  end

  it "127" do
    match 127, "\x18\x7f".b
  end

  it "128" do
    match 128, "\x18\x80".b
  end

  it "256" do
    match 256, "\x19\x01\x00".b
  end

  it "-1" do
    match -1, "\x20".b
  end

  it "-33" do
    match -33, "\x38\x20".b
  end

  it "-129" do
    match -129, "\x38\x80".b
  end

  it "-257" do
    match -257, "\x39\x01\x00".b
  end

  it "{1=>1}" do
    obj = {1=>1}
    match obj, "\xA1\x01\x01".b
  end

  it "1.0" do
    match 1.0, "\xF9\x3c\x00".b
  end

  it "NaN" do
    match Float::NAN, "\xF9\x7e\x00".b
  end

  it "[]" do
    match [], "\x80".b
  end

  it "[0, 1, ..., 14]" do
    obj = (0..14).to_a
    match obj, "\x8f\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e".b
  end

  it "[0, 1, ..., 15]" do
    obj = (0..15).to_a
    match obj, "\x90\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f".b
  end

  it "[0, 1, ..., 22]" do
    obj = (0..22).to_a
    match obj, "\x97\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16".b
  end

  it "[0, 1, ..., 23]" do
    obj = (0..23).to_a
    match obj, "\x98\x18\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17".b
  end

  it "{}" do
    obj = {}
    match obj, "\xA0".b
  end

  it "very simple bignums" do
    CBOR.decode("\xc2\x40").should == 0
    CBOR.decode("\xc2\x41\x00").should == 0
    CBOR.decode("\xc2\x41a").should == 97
    CBOR.decode("\xc2\x42aa").should == 24929
    CBOR.decode("\xc3\x40").should == ~0
    CBOR.decode("\xc3\x41\x00").should == ~0
    CBOR.decode("\xc3\x41a").should == ~97
    CBOR.decode("\xc3\x42aa").should == ~24929
  end

  it "0x982173487123985791827359872948752345" do
    check_bn(0x982173487123985791827359872948752345, 0xc2)
    check_bn(-0x982173487123985791827359872948752345, 0xc3)
    check_bn(0x9821734871239857918273598729487523, 0xc2)
    check_bn(0x98217348712398579182735987294875, 0xc2)
    check_bn(0x982173487123985791827359872948, 0xc2)
    check_bn(0x9821734871239857918273598729, 0xc2)
    check_bn(0x98217348712398579182735987, 0xc2)
    check 20, 0x982173487123985791827359872948752345
    check 20, -0x982173487123985791827359872948752345
    check 19, 0x9821734871239857918273598729487523
    check 18, 0x98217348712398579182735987294875
    check 18, -0x98217348712398579182735987294875
    check 17, 0x982173487123985791827359872948
    check 16, 0x9821734871239857918273598729
    check 15, 0x98217348712398579182735987
    check 15, 0x9821734871239857918273598
    check 16, [0x9821734871239857918273598]
    check 39, 0x982173487123985791827359872948752345982173487123985791827359872948752345
    check 3005, 256**3000+4711       # 3001 bignum, 3 string, 1 tag
  end

  it "fixnum/bignum switch" do
    CBOR.encode(CBOR.decode("\xc2\x40")).should == "\x00".b
    CBOR.encode(CBOR.decode("\xc2\x41\x00")).should == "\x00".b
    CBOR.encode(CBOR.decode("\xc2\x41a")).should == "\x18a".b
    CBOR.encode(CBOR.decode("\xc2\x42aa")).should == "\x19aa".b
    CBOR.encode(CBOR.decode("\xc2\x43aaa")).should == "\x1A\x00aaa".b
    CBOR.encode(CBOR.decode("\xc2\x44aaaa")).should == "\x1Aaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x45aaaaa")).should == "\e\x00\x00\x00aaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x46aaaaaa")).should == "\e\x00\x00aaaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x47aaaaaaa")).should == "\e\x00aaaaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x48aaaaaaaa")).should == "\eaaaaaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x49\x00aaaaaaaa")).should == "\eaaaaaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x49aaaaaaaaa")).should == "\xC2Iaaaaaaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x4a\x00aaaaaaaaa")).should == "\xC2Iaaaaaaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x4aaaaaaaaaaa")).should == "\xC2Jaaaaaaaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x4b\x00aaaaaaaaaa")).should == "\xC2Jaaaaaaaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x4baaaaaaaaaaa")).should == "\xC2Kaaaaaaaaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x4c\x00aaaaaaaaaaa")).should == "\xC2Kaaaaaaaaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x4caaaaaaaaaaaa")).should == "\xC2Laaaaaaaaaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x4d\x00aaaaaaaaaaaa")).should == "\xC2Laaaaaaaaaaaa".b
    CBOR.encode(CBOR.decode("\xc2\x4daaaaaaaaaaaaa")).should == "\xC2Maaaaaaaaaaaaa".b
  end

  it "a-non-ascii" do
    if ''.respond_to? :encode
      match "abc".encode("UTF-32BE"), "cabc"
    end
  end

  it "a" do
    match "a".encode_as_utf8, "aa"
    check 2, "a".encode_as_utf8
    check_decode "aa", "a".encode_as_utf8
  end
  
  it "a.b" do
    if ''.respond_to? :encode
      match "a".b, "Aa"
    end
    check 2, "a".b
    check_decode "Aa", "a".b
  end

  it "[_ ]" do
    check_decode "\x9f\xff", []
  end

  it "[_ 1]" do
    check_decode "\x9f\x01\xff", [1]
  end

  it "{_ }" do
    check_decode "\xbf\xff", {}
  end

  it "{_ 1 => 2}" do
    check_decode "\xbf\x01\x02\xff", {1 => 2}
  end


  it "{_ 1 => BREAK}" do
    lambda {
      check_decode "\xbf\x01\xff", {1 => 2}
    }.should raise_error(MessagePack::MalformedFormatError)
  end

  it "(_ a b)" do
    check_decode "\x7f\xff", "".encode_as_utf8
    check_decode "\x5f\xff", "".b
    check_decode "\x7faa\xff", "a".encode_as_utf8
    check_decode "\x5fAa\xff", "a".b
    check_decode "\x7faabbb\xff", "abb".encode_as_utf8
    check_decode "\x5fAaBbb\xff", "abb".b
    if ''.respond_to? :encode
      lambda {
        check_decode "\x7faaBbb\xff", "abb".encode_as_utf8
      }.should raise_error(MessagePack::MalformedFormatError)
      lambda {
        check_decode "\x7fAa\xff", "a".encode_as_utf8
      }.should raise_error(MessagePack::MalformedFormatError)
      lambda {
        check_decode "\x5fAabbb\xff", "abb".b
      }.should raise_error(MessagePack::MalformedFormatError)
      lambda {
        check_decode "\x5faa\xff", "a".b
      }.should raise_error(MessagePack::MalformedFormatError)
    end
  end

  it "(_ not-a-string)" do
    lambda {
      check_decode "\x5f\x00\xff", "".b
    }.should raise_error(MessagePack::MalformedFormatError)
  end

  it "bare break" do
    lambda {
      check_decode "\xff", "".b
    }.should raise_error(MessagePack::MalformedFormatError)
    lambda {
      check_decode "\x82\xff\x00\x00", "".b
    }.should raise_error(MessagePack::MalformedFormatError)
    lambda {
      check_decode "\x82\x9f\xff\xff", "".b
    }.should raise_error(MessagePack::MalformedFormatError)
  end

  it "Tagged" do
    expect { check 10, CBOR::Tagged.new("foo", 2) }.to raise_error(TypeError)
    check 2, CBOR::Tagged.new(10, 2)
    check 4, CBOR::Tagged.new(0xBEEF, 2)
    check 6, CBOR::Tagged.new(0xDEADBEEF, 2)
    check 10, CBOR::Tagged.new(0xDEADBEEFDEADBEEF, 2)
    expect { check 10, CBOR::Tagged.new(0x1DEADBEEFDEADBEEF, 2) }.to raise_error(RangeError)
  end

  it "Time" do
    check_decode "\xc1\x19\x12\x67", Time.at(4711)
    check 6, Time.at(Time.now.to_i)
  end

  it "URI" do
    # check_decode "\xd8\x20\x78\x13http://www.ietf.org", CBOR::Tagged.new(32, "http://www.ietf.org")
    require 'uri'
    check_decode "\xd8\x20\x78\x13http://www.ietf.org", URI.parse("http://www.ietf.org")
    # This doesn't work yet if 'uri' is not required before 'cbor':
    # check 6, URI.parse("http://www.ietf.org")
  end

  it "regexp" do
    check_decode "\xd8\x23\x63foo", /foo/
    check 14, /(?-mix:foo)/
    check 6, /foo/
  end

  it "Unknown simple 0" do
    check_decode "\xf3", CBOR::Simple.new(19)
    check 1, CBOR::Simple.new(0)
  end

  it "Unknown tag 0" do
    check_decode "\xc0\x00", CBOR::Tagged.new(0, 0)
    check 11, CBOR::Tagged.new(4711, "Frotzel")
  end

  it "Keys as Symbols" do       # Experimental!
    CBOR.decode(CBOR.encode({:a => 1}), :keys_as_symbols).should == {:a => 1}
    CBOR.decode(CBOR.encode({:a => 1})).should == {"a" => 1}
    expect { CBOR.decode("\x00", :foobar) }.to raise_error(ArgumentError)
  end

  it 'CBOR.decode symbolize_keys' do
    symbolized_hash = {:a => 'b', :c => 'd'}
    CBOR.decode(CBOR.encode(symbolized_hash), :symbolize_keys => true).should == symbolized_hash
  end

  it 'Unpacker#read symbolize_keys' do
    unpacker = Unpacker.new(:symbolize_keys => true)
    symbolized_hash = {:a => 'b', :c => 'd'}
    unpacker.feed(CBOR.encode(symbolized_hash)).read.should == symbolized_hash
  end

  it 'handle outrageous sizes 1' do
    expect { CBOR.decode("\xa1") }.to raise_error(EOFError)
    expect { CBOR.decode("\xba\xff\xff\xff\xff") }.to raise_error(EOFError)
    expect { CBOR.decode("\xbb\xff\xff\xff\xff\xff\xff\xff\xff") }.to raise_error(EOFError)
    expect { CBOR.decode("\xbb\x01\x01\x01\x01\x01\x01\x01\x01") }.to raise_error(EOFError)
    expect { CBOR.decode("\xbb\x00\x00\x01\x01\x01\x01\x01\x01") }.to raise_error(EOFError)
    expect { CBOR.decode("\x81") }.to raise_error(EOFError)
    expect { CBOR.decode("\x9a\xff\xff\xff\xff") }.to raise_error(EOFError)
  end
  it 'handle outrageous sizes 2' do
    expect { CBOR.decode("\x9b\xff\xff\xff\xff\xff\xff\xff\xff") }.to raise_error(EOFError)
  end
  it 'handle outrageous sizes 3' do
    expect { CBOR.decode("\x9b\x01\x01\x01\x01\x01\x01\x01\x01") }.to raise_error(EOFError)
  end
  it 'handle outrageous sizes 4' do
    expect { CBOR.decode("\x9b\x00\x00\x01\x01\x01\x01\x01\x01") }.to raise_error(EOFError)
    expect { CBOR.decode("\x61") }.to raise_error(EOFError)
    expect { CBOR.decode("\x7a\xff\xff\xff\xff") }.to raise_error(EOFError)
  end
  it 'handle outrageous sizes 5' do
    expect { CBOR.decode("\x7b\xff\xff\xff\xff\xff\xff\xff\xff") }.to raise_error(EOFError)
  end
  it 'handle outrageous sizes 6' do
    expect { CBOR.decode("\x7b\x01\x01\x01\x01\x01\x01\x01\x01") }.to raise_error(EOFError)
  end
  it 'handle outrageous sizes 7' do
    expect { CBOR.decode("\x7b\x00\x00\x01\x01\x01\x01\x01\x01") }.to raise_error(EOFError)
    expect { CBOR.decode("\x41") }.to raise_error(EOFError)
    expect { CBOR.decode("\x5a\xff\xff\xff\xff") }.to raise_error(EOFError)
  end
  it 'handle outrageous sizes 8' do
    expect { CBOR.decode("\x5b\xff\xff\xff\xff\xff\xff\xff\xff") }.to raise_error(EOFError)
  end
  it 'handle outrageous sizes 9' do
    expect { CBOR.decode("\x5b\x01\x01\x01\x01\x01\x01\x01\x01") }.to raise_error(EOFError)
  end
  it 'handle outrageous sizes 10' do
    expect { CBOR.decode("\x5b\x00\x00\x01\x01\x01\x01\x01\x01") }.to raise_error(EOFError)
  end


## FIXME
#  it "{0=>0, 1=>1, ..., 14=>14}" do
#    a = (0..14).to_a;
#    match Hash[*a.zip(a).flatten], "\x8f\x05\x05\x0b\x0b\x00\x00\x06\x06\x0c\x0c\x01\x01\x07\x07\x0d\x0d\x02\x02\x08\x08\x0e\x0e\x03\x03\x09\x09\x04\x04\x0a\x0a"
#  end
#
#  it "{0=>0, 1=>1, ..., 15=>15}" do
#    a = (0..15).to_a;
#    match Hash[*a.zip(a).flatten], "\xde\x00\x10\x05\x05\x0b\x0b\x00\x00\x06\x06\x0c\x0c\x01\x01\x07\x07\x0d\x0d\x02\x02\x08\x08\x0e\x0e\x03\x03\x09\x09\x0f\x0f\x04\x04\x0a\x0a"
#  end

## FIXME
#  it "fixmap" do
#    check_map 1, 0
#    check_map 1, (1<<4)-1
#  end
#
#  it "map 16" do
#    check_map 3, (1<<4)
#    check_map 3, (1<<16)-1
#  end
#
#  it "map 32" do
#    check_map 5, (1<<16)
#    #check_map 5, (1<<32)-1  # memory error
#  end

  def check(len, obj)
    raw = obj.to_cbor.to_s
    raw.length.should == len
    obj2 = MessagePack.unpack(raw)
    obj2.should == obj
    if obj.respond_to? :encoding
      obj2.encoding.should == obj.encoding
    end
  end

  def check_raw(overhead, num)
    check num+overhead, " "*num
  end

  def check_array(overhead, num)
    check num+overhead, Array.new(num)
  end

  def check_bn(bn, tag)
    bnb = bignum_to_bytes(bn < 0 ? ~bn : bn)
    bnbs = bnb.size
    match bn, "#{tag.chr}#{(bnbs + 0x40).chr}#{bnb}"
  end

  def match(obj, buf)
    raw = obj.to_cbor.to_s
    raw.should == buf
  end

  def check_decode(c, obj)
    obj2 = MessagePack.unpack(c)
    obj2.should == obj
    if obj.respond_to? :encoding
      obj2.encoding.should == obj.encoding
    end
  end

end

