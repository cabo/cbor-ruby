module CBOR
  #
  # Serializes an object into an IO or String.
  #
  # @overload encode(obj)
  #   @return [String] serialized data
  #
  # @overload encode(obj, io)
  #   @return [IO]
  #
  def self.encode(arg)
  end

  #
  # Serializes an object into an IO or String. Alias of encode.
  #
  # @overload dump(obj)
  #   @return [String] serialized data
  #
  # @overload dump(obj, io)
  #   @return [IO]
  #
  def self.dump(arg)
  end

  #
  # Serializes an object into an IO or String. Alias of encode.
  #
  # @overload dump(obj)
  #   @return [String] serialized data
  #
  # @overload dump(obj, io)
  #   @return [IO]
  #
  def self.pack(arg)
  end

  #
  # Deserializes an object from an IO or String.
  #
  # @overload decode(string)
  #   @param string [String] data to deserialize
  #
  # @overload decode(io)
  #   @param io [IO]
  #
  # @return [Object] deserialized object
  #
  def self.decode(arg)
  end

  #
  # Deserializes an object from an IO or String. Alias of decode.
  #
  # @overload load(string)
  #   @param string [String] data to deserialize
  #
  # @overload load(io)
  #   @param io [IO]
  #
  # @return [Object] deserialized object
  #
  def self.load(arg)
  end

  #
  # Deserializes an object from an IO or String. Alias of decode.
  #
  # @overload unpack(string)
  #   @param string [String] data to deserialize
  #
  # @overload unpack(io)
  #   @param io [IO]
  #
  # @return [Object] deserialized object
  #
  def self.unpack(arg)
  end
end

