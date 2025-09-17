#pragma once
#ifndef DR_SOCKET_SOCKET_RB_H
#define DR_SOCKET_SOCKET_RB_H

static const char * ruby_socket_code = R"(

module OService
    def self.__exec_callback(data)
        if data.nil?
            return
        end

        to_call = method(:oservice_update)
        if to_call.nil?
            raise OServiceCallbackMethodNotFound.new
            return
        end

        if data.kind_of?(Array)
            data.each do |d|
                type = d["type"].to_sym
                real_data = d["data"]
                to_call.call(type, real_data)
            end
            return
        end

        if data.kind_of?(Hash)
            d = data
            type = d["type"].to_sym
            real_data = d["data"]
            to_call.call(type, real_data)
            return
        end

        raise OServiceWrongDataTypeException.new
    end
end

def shutdown args

end

module GTK
    class Runtime
        old_sdl_tick = instance_method(:__sdl_tick__)

        define_method(:__sdl_tick__) do |args|
            old_sdl_tick.bind(self).(args)

            OService.__update_service
        end
    end
end

class OServiceWrongDataTypeException < RuntimeError
  def initialize()
  end
end

class OServiceCallbackMethodNotFound < RuntimeError
  def initialize()
  end
end

)";

#endif