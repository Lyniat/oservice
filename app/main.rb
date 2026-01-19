require 'app/lib/lib.rb'
require 'app/help.rb'

$test_data = {
  "this_is_data?" => true,
  :letters => [
    "a",
    "b",
    "c",
    "d",
  ],
  "numbers" => [
    [1, 1.1],
    [2, 2.2],
    [-3, -3.3],
  ],
  "random_string_in_between": "--",
  "dev_info" => {
    "gender" => "raccoon",
    :age => 256,
    "name" => "lyniat",
    "knows_how_to_code" => true,
    "knows_how_to_code_good" => false,
    :credit_card => {
      "type" => "Coders Club",
      "card number" => "0016 0032 0064 0128",
    },
  },
  "what_causes_exceptions?": nil,
  "different_keys": {
    567 => "567",
    "abc" => "ABC",
    3.14 => "PI",
  },
  "end_of_data" => true,
}

def boot (args)
  $gtk.dlopen 'oservice'
  OService.init_api
end

def oservice_update(type, update_value)
  state = gtk_state
  if type == :on_data_received

  elsif type == :on_lobby_data_changed

  elsif type == :on_lobby_self_joined
    puts update_value
    state.in_lobby = true
    state.lobby_name = update_value["name"]
    state.lobbies = []
    state.draw_lobbies = []
  elsif type == :on_lobby_self_left
    puts update_value
    state.in_lobby = false
    state.lobby_name = nil
    state.chat_history = []
  elsif type == :on_lobby_player_joined

  elsif type == :on_lobby_player_left

  elsif type == :on_lobby_list
    puts update_value
    state.lobbies = update_value
  elsif type == :on_lobby_chat
    puts update_value
    state.chat_history << "#{update_value['member']}: #{update_value['text']}"
  elsif type == :on_lobby_name_changed

  elsif type == :on_lobby_max_players_changed

  elsif type == :on_lobby_info_fetched

  elsif type == :on_lobby_created

  elsif type == :on_lobby_member_data_changed

  elsif type == :on_lobby_member_name_changed

  elsif type == :on_lobby_file_added

  elsif type == :on_lobby_file_removed

  elsif type == :on_lobby_file_requested

  elsif type == :on_lobby_file_data_send_progress

  elsif type == :on_lobby_file_data_send_finished

  elsif type == :on_lobby_file_data_receive_progress

  elsif type == :on_lobby_file_data_receive_finished

  end
end

def init_app
  state = gtk_state
  state.in_lobby = false
  state.lobby_owner = false
  state.lobby_name = nil
  state.user_name = OService.get_local_name
  state.local_ip = OService.get_local_ip

  state.buttons = {}

  state.lobbies = []
  state.draw_lobbies = []

  state.chat_history = []
end

def tick (args)
  if gtk_ticks.zero?
    init_app
  end

  draw_ui(args)
end

def draw_ui(args)
  state = gtk_state
  gtk_background([50, 50, 50])

  if state.lobby_name.nil?
    lobby_name = "Not in a lobby!"
  else
    lobby_name = state.lobby_name
  end

  state.info_label ||= Label.new(0, 0, 10, 1, "nil", 4)
  state.info_label.text = "Lobby: #{lobby_name}"
  state.info_label.draw

  state.name_label ||= Label.new(10, 0, 5, 1, "Name: #{state.user_name}", 4)
  state.name_label.draw

  state.ip_label ||= Label.new(15, 0, 5, 1, "IPv4: #{state.local_ip}", 4)
  state.ip_label.draw

  state.button_console ||= Button.new(20, 0, 4, 1, "CONSOLE", :cb_open_console)
  state.button_console.draw

  buttons = state.buttons

  buttons.list_lobbies ||= Button.new(0, 1, 4, 1, "List lobbies", :cb_list_lobbies)
  buttons.list_lobbies.draw

  buttons.leave_lobby ||= Button.new(4, 1, 4, 1, "Leave lobby", :cb_leave_lobby)
  if state.in_lobby
    buttons.leave_lobby.draw
  end

  state.available_label ||= Label.new(0, 2, 10, 1, "Available lobbies:", 4)
  state.available_label.draw

  lobbies = state.lobbies
  draw_lobbies = state.draw_lobbies
  max_lobbies = lobbies.size > 3 ? 3 : lobbies.size
  (max_lobbies).each do |i|
    lobby = lobbies[i]
    draw_lobbies[i] ||= Button.new(0, 3 + i, 4, 1, "#{lobby['lobby_name']}", :cb_join_lobby, lobby)
    draw_lobbies[i].draw
  end

  state.chat_text ||= Label.new(0, 5, 6, 5, "", 4)
  chat_history = state.chat_history
  max_chat_history = chat_history.size > 5 ? 5 : chat_history.size
  chat_text = ""
  (max_chat_history).each do |i|
    chat_text += "#{chat_history[i]}\n"
    state.chat_text.text = chat_text
  end

  state.chat_label ||= Label.new(0, 10, 4, 1, "Chat:", 4)
  state.chat_input ||= TextInput.new(4, 10, 14, 1, "", :cb_input)

  if state.in_lobby
    state.chat_label.draw
    state.chat_input.draw
    state.chat_text.draw
  end
end

def cb_input (data)
  OService.send_chat(data)
  state.chat_input.text = ""
end

def cb_leave_lobby
  OService.leave_lobby
end

def cb_join_lobby (data)
  entries = data["entries"]
  if entries.any?
    entry = entries[0]
    id = entry["id"].to_s
    OService.join_lobby_by_id(id)
  end
end

def cb_list_lobbies
  OService.list_lobbies
end

def cb_open_console
  GTK.show_console
end