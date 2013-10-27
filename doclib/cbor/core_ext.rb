
class NilClass
  #
  # Same as CBOR.encode(self[, io]).
  #
  # @return [String] serialized data
  #
  def to_cbor(io=nil)
  end
end

class TrueClass
  #
  # Same as CBOR.encode(self[, io]).
  #
  # @return [String] serialized data
  #
  def to_cbor(io=nil)
  end
end

class FalseClass
  #
  # Same as CBOR.encode(self[, io]).
  #
  # @return [String] serialized data
  #
  def to_cbor(io=nil)
  end
end

class Fixnum < Integer
  #
  # Same as CBOR.encode(self[, io]).
  #
  # @return [String] serialized data
  #
  def to_cbor(io=nil)
  end
end

class Bignum < Integer
  #
  # Same as CBOR.encode(self[, io]).
  #
  # @return [String] serialized data
  #
  def to_cbor(io=nil)
  end
end

class Float < Numeric
  #
  # Same as CBOR.encode(self[, io]).
  #
  # @return [String] serialized data
  #
  def to_cbor(io=nil)
  end
end

class String
  #
  # Same as CBOR.encode(self[, io]).
  #
  # @return [String] serialized data
  #
  def to_cbor(io=nil)
  end
end

class Array
  #
  # Same as CBOR.encode(self[, io]).
  #
  # @return [String] serialized data
  #
  def to_cbor(io=nil)
  end
end

class Hash
  #
  # Same as CBOR.encode(self[, io]).
  #
  # @return [String] serialized data
  #
  def to_cbor(io=nil)
  end
end

class Time
  #
  # Same as CBOR.encode(self[, io]).
  #
  # @return [String] serialized data
  #
  def to_cbor(io=nil)
  end
end


class Regexp
  #
  # Same as CBOR.encode(self[, io]).
  #
  # @return [String] serialized data
  #
  def to_cbor(io=nil)
  end
end

class URI
  #
  # Same as CBOR.encode(self[, io]).  This method is only defined if
  # the class URI was defined when the CBOR extension was loaded, so
  # +require "uri"+ before +require "cbor"+ if you need this.
  #
  # @return [String] serialized data
  #
  def to_cbor(io=nil)
  end
end

class Symbol
  #
  # Same as CBOR.encode(self[, io]).
  #
  # @return [String] serialized data
  #
  def to_cbor(io=nil)
  end
end
