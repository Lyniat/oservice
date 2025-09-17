#pragma once

constexpr const char* InvalidVisibilityError = "Invalid type. Expected ':os_private' or ':os_public'.";
constexpr const char* InvalidPacketTypeError = "Invalid type. Expected ':os_reliable' or ':os_unreliable'.";
constexpr const char* InvalidServiceTypeError = "Invalid type. Expected ':os_steam' or ':os_enet'.";
constexpr const char* BinaryBufferError = "BinaryBuffer failed.";
constexpr const char* SerializationError = "Serialization failed.";
constexpr const char* DeserializationError = "Deserialization failed.";

#define ERR_INV_VISIBILITY API->mrb_raise(state, exception_invalid_visibility, InvalidVisibilityError);
#define ERR_INV_PACKET API->mrb_raise(state, exception_invalid_packet, InvalidPacketTypeError);
#define ERR_INV_SERVICE API->mrb_raise(state, exception_invalid_service, InvalidServiceTypeError);
#define ERR_BIN_BUFF API->mrb_raise(state, exception_bin_buffer, BinaryBufferError);

static const char* ruby_exceptions_code = R"(

module OService
  class BinaryBufferError < StandardError
    def initialize(msg)
      super
    end
  end

  class InvalidPacketTypeError < StandardError
      def initialize(msg)
        super
      end
  end

  class InvalidVisibilityError < StandardError
      def initialize(msg)
        super
      end
  end

  class SerializationError < StandardError
      def initialize(msg)
        super
      end
  end

  class InvalidServiceTypeError < StandardError
      def initialize(msg)
        super
      end
  end
end

)";