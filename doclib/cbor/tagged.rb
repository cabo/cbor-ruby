module CBOR

  #
  # CBOR::Tagged is used to carry around tagged values that don't map
  # into Ruby classes (false, true, nil are the ones that do).
  class Tagged

    # @param tag [Integer] The integer number of the tag
    # @param value [Object] The value that is being tagged
    def initialize(tag, value)
    end

    attr_reader :tag, :value

  end
end
