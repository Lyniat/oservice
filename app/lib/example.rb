require 'app/lib/button.rb'
require 'app/lib/label.rb'
require 'app/lib/text_input.rb'
require 'app/lib/checkbox.rb'
require 'app/lib/radio.rb'
require 'app/lib/image_button.rb'
require 'app/lib/dbg_ui.rb'

def draw_ui(args)
   args.outputs.background_color = [50, 50, 50]
  args.state.checkbox ||= Checkbox.new(2, 5, 1, 1, true)
  args.state.checkbox.draw

  args.state.info_label ||= Label.new(2, 4, 8, 1, "this is a label with text", 4)
  args.state.info_label.draw

  args.state.text_input ||= TextInput.new(2, 6, 8, 1, "My text...", :callback_from_input, 4, 7.89) # "…" seems not to work with default font
  args.state.text_input.draw

  args.state.button_0 ||= Button.new(2, 2, 4, 2, "No callback")
  args.state.button_0.draw

  args.state.button_1 ||= Button.new(6, 2, 4, 2, "Callback", :simple_callback)
  args.state.button_1.draw

  args.state.button_2 ||= Button.new(10, 2, 4, 2, "Callback with args", :callback_with_args, args.state.info_label, args.state.text_input, args.state.checkbox)
  args.state.button_2.draw

  args.state.image_button_height = 3
  args.state.image_button ||= ImageButton.new(10, 4, 4, args.state.image_button_height, "sprites/raccoon.jpg")
  args.state.image_button.draw

  radio_list = []
  args.state.radio_0 ||= Radio.new(14, 2, 1, 1, radio_list, false, :radio_change, args.state.image_button, 1)
  args.state.radio_1 ||= Radio.new(14, 3, 1, 1, radio_list, false, :radio_change, args.state.image_button, 2)
  args.state.radio_2 ||= Radio.new(14, 4, 1, 1, radio_list, true, :radio_change, args.state.image_button, 3)
  args.state.radio_3 ||= Radio.new(14, 5, 1, 1, radio_list, false, :radio_change, args.state.image_button, 4)
  args.state.radio_0.draw
  args.state.radio_1.draw
  args.state.radio_2.draw
  args.state.radio_3.draw

end

def test_callback(input)
  puts input
end

def simple_callback
  puts "simple callback"
end

def callback_with_args(label, input, checkbox)
  label.text = "Input: #{input.text}, checkbox: #{checkbox.enabled}"
end

def callback_from_input(str, num_0, num_1)
  puts "input: #{str}, #{num_0}, #{num_1}"
end

def radio_change(image_button, height)
  image_button.h = height
end
