require 'set'
require 'pstore'
require 'ntx'

class Notetag
  def initialize(id, tags, body)
    @id   = id
    @tags = tags
    @body = body
  end
end

class Response
  attr_writer :request_id
end

class NTXServer

  ID_BITMASK = 1048575

  def initialize(uds_path, store_path)
    @note_by_id   = Hash.new
    @group_by_tag = Hash.new
    @touched      = false

    # Load the list of notes from the file.
    # Dump the notes into the relevant categories.

    @transport    = UNIXTransport.new(uds_path, true)
  end

  def server_loop
    loop do
      request = @transport.recv

      response = begin
        case request.class
        when AddRequest
          AddResponse.new(define_note(request.tags, request.body))
        when GetRequest
          GetResponse.new(retrieve_note(request.id))
        when DelRequest
          DelResponse.new(remove_note(request.id))
        when EditRequest
          remove_note(request.note.id)
          EditResponse.new(define_note(request.note.tags, request.note.body,
                                       request.note.id))
        when ListRequest
          ListResponse.new(list_category(request.tags))
        else
          ErrorResponse.new(nil)
        end
      rescue => err
        # Should we send back the exception, or simply nil?
        ErrorResponse.new(err)
      end

      response.request_id = request.request_id
      @transport.send response
    end
  end

  def flush
    if @touched
      # ...
    end
  end

  def close
    @transport.close
  end

  private
  def define_note(tags, body, id = nil)
    # Create a unique ID, 
    new_id = body.hash & ID_BITMASK unless id
    while(@note_by_id[new_id])
      new_id = new_id + 1
    end

    note = Notetag.new(new_id, tags, body)

    # Push the new tag into ID hash.
    @note_by_id[note.id] = note

    # Push it into each tag group, creating Sets as necessary.
    tags.each do |tag|
      (@group_by_tag[tags.to_sym] ||= Set.new) << note
    end

    note.id
  end

  def remove_note(id)
    note = @note_by_id[id]

    # Remove the note from each tag group.
    note.tags.each do |tags|
      @group_by_tag[tags.to_sym].delete(note)
    end

    # Remove the note from the ID hash.
    @note_by_id.delete(id)

    note.id
  end

  def retrieve_note(id)
    @note_by_id[id]
  end

  def list_category(tags)
  end
end

if __FILE__ == $0
  server = NTXServer.new("/tmp/ntxserve", "")
  begin
    server.server_loop
  rescue Interrupt
    server.close
  end
end
