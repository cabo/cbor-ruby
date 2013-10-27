require 'spec_helper'
require 'json'

describe MessagePack do
  here = File.dirname(__FILE__)
  CASES         = File.read("#{here}/cases.cbor_stream")
  CASES_JSON    = File.read("#{here}/cases.json")
  CASES_INDEFINITE = File.read("#{here}/cases.cbor_stream") # TODO

  it 'compare with json' do
    ms = []
    MessagePack::Unpacker.new.feed_each(CASES) {|m|
      ms << m
    }

    js = JSON.load(CASES_JSON)

    ms.zip(js) {|m,j|
      m.should == j
    }
  end

  it 'compare with compat' do
    ms = []
    MessagePack::Unpacker.new.feed_each(CASES) {|m|
      ms << m
    }

    cs = []
    MessagePack::Unpacker.new.feed_each(CASES_INDEFINITE) {|c|
      cs << c
    }

    ms.zip(cs) {|m,c|
      m.should == c
    }
  end
end

