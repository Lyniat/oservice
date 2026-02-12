# horizontal: ⇐⇒
# vertical: ⇑⇓
module Alignment
    LEFT = 0
    CENTER = 1
    RIGHT = 2
    TOP = 0
    BOTTOM = 2
end

module Blendmode
    NONE = 0
    ALPHA = 1
    ADD = 2
    MOD = 3
    MUL = 4
end

module Scale
    NEAR = 0
    LINEAR = 1
    AA = 2
end

def gtk_label(obj) = $args.outputs.labels << obj
def gtk_sprite(obj) = $args.outputs.sprites << obj
def gtk_solid(obj) = $args.outputs.solids << obj
def gtk_border(obj) = $args.outputs.borders << obj
def gtk_line(obj) = $args.outputs.lines << obj
def gtk_render(obj) = $args.outputs.primitives << obj
def gtk_background(h) = $args.outputs.background_color = h

def gtk_sound(obj) = $args.outputs.sounds << obj

def gtk_music_start(name, obj) = $args.audio[name] = obj
def gtk_music_stop(name) = $args.audio[name] = nil
def gtk_music(name) = $args.audio[name]

def gtk_keyboard = $args.inputs.keyboard
def gtk_keyboard_down = $args.inputs.keyboard.key_down
def gtk_keyboard_up = $args.inputs.keyboard.key_up
def gtk_keyboard_held = $args.inputs.keyboard.key_held

def gtk_mouse = $args.inputs.mouse
def gtk_mouse_down = $args.inputs.mouse.key_down
def gtk_mouse_up = $args.inputs.mouse.key_up
def gtk_mouse_held = $args.inputs.mouse.key_held
def gtk_mouse_click = $args.inputs.mouse.click

def gtk_input = $args.inputs
def gtk_input_dir_vector = $args.inputs.keyboard.directional_vector

def gtk_state = $args.state
def gtk_ticks = $args.state.tick_count

def gtk_sin(rad) = Math.sin(rad)
def gtk_cos(rad) = Math.cos(rad)
def gtk_atan2(y, x) = Math.atan2(y, x)
def gtk_sqrt(v) = Math.sqrt(v)
