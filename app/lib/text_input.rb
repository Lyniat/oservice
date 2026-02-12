class TextInput
  attr_accessor :text, :callback, :arguments

  def initialize(x, y, w, h, text, *callback_and_args)
    @x = x
    @y = y
    @w = w
    @h = h
    @rect = GTK.args.layout.rect(col: @x, row: @y, w: @w, h: @h)
    @text = text
    @callback = callback_and_args.shift
    @arguments = callback_and_args
    @active = false
  end

  def draw
    args = GTK.args
    if args.inputs.mouse.click
      @active = args.inputs.mouse.inside_rect?(@rect)
    end

    if @active
      if args.inputs.keyboard.key_down.backspace
        @text = @text.chop
      elsif args.inputs.keyboard.key_down.enter
        trigger
      else
        input_char = args.inputs.keyboard.key_down.char
        @text += input_char if !input_char.nil?
      end
    end

    alpha = 180
    g = @active ? 160 : 120
    args.outputs.primitives << @rect.merge(primitive_marker: :solid,
                                        r: 100,
                                        g: g,
                                        b: 120,
                                        a: alpha)
    text_with_cursor = @text + (@active && GTK.args.state.tick_count % 40 >= 20 ? "|" : " ")
    args.outputs.primitives << @rect.center.merge(text: text_with_cursor,
                                              r: 255,
                                              g: 255,
                                              b: 255,
                                              anchor_x: 0.5,
                                              anchor_y: 0.5,
                                              font: DBGUI.font)
  end

  def trigger
    if !callback.nil?
      if arguments.nil?
        method(@callback).call(@text)
      else
        method(@callback).call(@text, *@arguments)
      end
    end
  end

  def reset
    @text = ""
  end

  def length
    @text.length
  end

  def active?
    @active
  end

  def x
    @x
  end

  def x=(x)
    @x = x
    @rect = GTK.args.layout.rect(col: @x, row: @y, w: @w, h: @h)
  end

  def y
    @y
  end

  def y=(y)
    @y = y
    @rect = GTK.args.layout.rect(col: @x, row: @y, w: @w, h: @h)
  end

  def w
    @w
  end

  def w=(w)
    @w = w
    @rect = GTK.args.layout.rect(col: @x, row: @y, w: @w, h: @h)
  end

  def h
    @h
  end

  def h=(h)
    @h = h
    @rect = GTK.args.layout.rect(col: @x, row: @y, w: @w, h: @h)
  end
end