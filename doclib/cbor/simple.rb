module CBOR

  #
  # CBOR::Simple is used to carry around simple values that don't map
  # into Ruby classes (false, true, nil are the ones that do).
  class Simple

    # @param value [Integer] The integer number of the simple value
    def initialize(value)
    end

    attr_reader :value

  end
end
