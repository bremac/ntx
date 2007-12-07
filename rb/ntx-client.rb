require 'ntx'

class Request
  attr_writer :request_id
end

class RequestError < StandardError
end

class NTXClient
  def initialize(server)
    # Ruby can't run out of request IDs, as our
    # chances of overflowing a BigNum are _very_
    # slim, so simply maintain a monotonically
    # incrementing ID count.
    @max_id   = 0

    @requests = Hash.new
    @handlers = Hash.new
    @locks    = Array.new # Currently unused.

    @server   = UNIXTransport.new(server)
  end

  def receive
    response = @server.recv
    request  = @requests.delete(response.request_id)

    unless request
      raise RequestError, "Unknown request ID #{response.request_id}."
    end
    raise response.err if response.class == ErrorResponse

    @handlers[response.class].call(request, response)
  end

  def handle_response(klass, &block)
    @handlers[klass] = block
  end
  
  def list(tags)
    send_request ListRequest.new(tags)
  end

  def get(id)
    send_request GetRequest.new(id)
  end

  def tags
    send_request TagsRequest.new
  end

  def add(tags, body)
    send_request AddRequest.new(tags, body)
  end

  def del(id)
    send_request DelRequest.new(id)
  end

  private
  def send_request(req)
    req.request_id      = @max_id
    @requests[@max_id]  = req
    @max_id            += 1

    @transport.send req
  end
end

if __FILE__ == $0
  client = NTXClient.new("/tmp/ntxserve")

  client.handle_response(AddResponse) do |req, res|
  end

  client.handle_response(GetResponse) do |req, res|
  end

  client.handle_response(DelResponse) do |req, res|
  end

  client.handle_response(EditResponse) do |req, res|
  end

  client.handle_response(ListResponse) do |req, res|
  end

  client.handle_response(TagsResponse) do |req, res|
  end
 
  # REPL-style loop.
  # ... 
end
