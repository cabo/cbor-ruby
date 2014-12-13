require 'rubygems'              # for 1.8

RSpec.configure do |config|
  config.expect_with :rspec do |c|
    c.syntax = [:should, :expect]
  end
  config.mock_with :rspec do |c|
    c.syntax = [:should, :expect]
  end
end


class String
  if ''.respond_to? :encode
    def encode_as_utf8
      encode(Encoding::UTF_8)
    end
    def force_as_utf8
      force_encoding(Encoding::UTF_8)
    end
    unless String.instance_methods.include?(:b)
      def b
        dup.force_encoding(Encoding::BINARY)
      end
    end
  else
    def encode_as_utf8
      self                      # MRI 1.8
    end
    def force_as_utf8
      self                      # MRI 1.8
    end
    def b
      self
    end
  end
  unless ''.respond_to? :clear
    def clear
      replace('')
    end
  end
  def hexbytes(sep = '')
    bytes.map{|x| "%02x" % x}.join(sep)
  end
end

if ENV['SIMPLE_COV']
  require 'simplecov'
  SimpleCov.start do
    add_filter 'spec/'
    add_filter 'pkg/'
    add_filter 'vendor/'
  end
end

if ENV['GC_STRESS']
  puts "enable GC.stress"
  GC.stress = true
end

require 'cbor'

MessagePack = CBOR              # XXX

Packer = MessagePack::Packer
Unpacker = MessagePack::Unpacker
Buffer = MessagePack::Buffer

