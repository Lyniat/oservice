#pragma once

#include <ossp/api.h>
#include <ossp/help.h>

constexpr const char* InvalidVisibilityError = "Invalid type. Expected ':os_private' or ':os_public'.";
constexpr const char* InvalidPacketTypeError = "Invalid type. Expected ':os_reliable' or ':os_unreliable'.";
constexpr const char* InvalidServiceTypeError = "Invalid type. Expected ':os_steam' or ':os_enet'.";
constexpr const char* ByteBufferError = "ByteBuffer failed.";
constexpr const char* SerializationError = "Serialization failed.";
constexpr const char* DeserializationError = "Deserialization failed.";

#define ERR_INV_VISIBILITY mrb_raise(state, exception_invalid_visibility, InvalidVisibilityError);
#define ERR_INV_PACKET mrb_raise(state, exception_invalid_packet, InvalidPacketTypeError);
#define ERR_INV_SERVICE mrb_raise(state, exception_invalid_service, InvalidServiceTypeError);
#define ERR_BIN_BUFF mrb_raise(state, exception_byte_buffer, ByteBufferError);

static const char* ruby_exceptions_code = R"(

module OService
  class ByteBufferError < StandardError
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