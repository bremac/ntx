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

  # Constant definitions.
  ID_BITMASK = 1048575

  def initialize(uds_path, store_path)
    @note_by_id   = Hash.new
    @group_by_tag = Hash.new
    @touched      = false
    @transport    = UNIXTransport.new(uds_path, true)

    # Load the list of notes from the file, and dump
    # the notes into the relevant categories.
    @store = PStore.new(store_path)
    @store.transaction(true) do
      if @store[:list]
        @store[:list].each do |note|
          define_note(note.tags, note.body, note.id)
        end
      end
    end
  end

  def server_loop
    loop do
      request = @transport.recv

      response = begin
        case request.class
        when AddRequest
          tags = request.tags.collect {|t| t.strip.downcase.to_sym}
          AddResponse.new define_note(tags, request.body)
        when GetRequest
          GetResponse.new retrieve_note(request.id)
        when DelRequest
          DelResponse.new remove_note(request.id)
        when EditRequest
          remove_note request.note.id
          EditResponse.new define_note(request.note.tags, request.note.body,
                                       request.note.id)
        when ListRequest
          tags = request.tags.collect {|t| t.strip.downcase.to_sym}
          ListResponse.new list_category(tags)
        when TagsRequest
          TagsResponse.new retrieve_tags
        else
          ErrorResponse.new nil
        end
      rescue => err
        # Should we send back the exception, or simply nil?
        ErrorResponse.new err
      end

      response.request_id = request.request_id
      @transport.send response
    end
  end

  def save
    if @touched
      @store.transaction do
        @store[:list] = @note_by_id.values
      end
    end
  end

  def close
    @transport.close
  end

  private
  def define_note(tags, body, id = nil)
    # Create a unique ID, if we require one.
    unless id
      new_id = body.hash & ID_BITMASK
      while(@note_by_id[new_id])
        new_id = new_id + 1
      end
    end

    note = Notetag.new(new_id, tags, body)

    # Push the new tag into ID hash.
    @note_by_id[note.id] = note

    # Push it into each tag group, creating tables as necessary.
    tags.each do |tag|
      group = (@group_by_tag[tags] ||= Hash.new)
      group[note.id] = note
    end

    @touched = true
    note.id
  end

  def remove_note(id)
    note = @note_by_id[id]

    # Remove the note from each tag group.
    # Delete each affected tag group should this make it empty.
    note.tags.each do |tag|
      @group_by_tag[tag].delete(note.id)
      @group_by_tag.delete(tag) if @group_by_tag.length == 0
    end

    # Remove the note from the ID hash.
    @note_by_id.delete(id)

    @touched = true
    note.id
  end

  def retrieve_note(id)
    @note_by_id[id]
  end

  def retrieve_tags
    @group_by_tag.keys
  end

  def list_category(tags)
    # If the tags array is empty, return all notes.
    return @notes_by_id.values if tags.length == 0

    # Collect all of the tag groups.
    groups = tags.collect {|tag| @group_by_tag[tag]}

    # Shorter execution path for easier searches.
    return nil       if groups.compact! || req.length == 0
    return groups[0] if groups.length == 1

    # Sort, so we only need to iterate the shortest set.
    groups.sort! {|a, b| a.length <=> b.length}

    # Find the intersection of all of the sets by iterating the shortest,
    # which greatly reduces repeated checks. We use (unencouraged)
    # metaprogramming to remove an inner loop by replacing the closure
    # with a proc created via eval.
    proto_proc = "Proc.new {|note| "
    (1...groups.length-1).each do |idx|
      proto_proc << "groups[#{idx}].include?(note.id) &&"
    end
    proto_proc << "groups#{groups.length}.include?(note.id)}"

    groups[0].values.select &(Object.instance_eval proto_proc)
  end
end

if __FILE__ == $0
  server = NTXServer.new("/tmp/ntxserve", ENV["HOME"]+"/.ntxdb")
  begin
    server.server_loop
  rescue Exception
    server.close
  end
  server.save
end
