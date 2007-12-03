require 'socket'

class Notetag
  attr_reader :id
  attr_accessor :tags, :body
end

class ClosedStruct
  def self.derive(*fields)
    Class.new(self) do
      fields.each {|f| attr_reader f}
      self.class_eval <<-END
        def initialize(#{fields.join ","})
          super
          #{fields.map {|f| "@#{f} = #{f}"}}
        end
      END
    end
  end
end

class Request < ClosedStruct
  attr_reader :request_id
end

class Response < ClosedStruct
  attr_reader :request_id
end

AddRequest   = Request.derive(:tags, :body)
GetRequest   = Request.derive(:id)
DelRequest   = Request.derive(:id)
EditRequest  = Request.derive(:note)
ListRequest  = Request.derive(:tags)

AddResponse   = Response.derive(:id)
GetResponse   = Response.derive(:note)
DelResponse   = Response.derive(:id)
EditResponse  = Response.derive(:id)
ListResponse  = Response.derive(:list)
ErrorResponse = Response.derive(:status)

class UNIXTransport
  def initialize(path, server = false)
    if server
      @listener = UNIXServer.new(path)
    else
      @socket = UNIXSocket.new(path)
    end
  end

  def await_connection
    if @listener
      @listener.listen(0)
      @socket = @listener.accept
    end
    nil
  end
  private :await_connection

  def recv
    # Connections are only made while receiving.
    await_connection unless @socket

    begin
      Marshal.load(@socket)
    rescue Errno::ECONNABORTED, Errno::ETIMEDOUT
      @socket.shutdown
      @socket = nil
    end
  end

  def send(obj)
    begin
      Marshal.dump(obj, @socket)
    rescue Errno::ECONNABORTED, Errno::ETIMEDOUT
      @socket.shutdown
      @socket = nil
    end
  end

  def close
    if @socket
      @socket.shutdown
      @socket = nil
    end

    if @listener
      @listener.shutdown
      File.unlink @listener.path
      @listener = nil
    end
  end
end
