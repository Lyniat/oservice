class Checkbox
  attr_accessor :enabled, :callback, :arguments

  def initialize(x, y, w, h, enabled, *callback_and_args)
    @x = x
    @y = y
    @w = w
    @h = h
    @rect = GTK.args.layout.rect(col: @x, row: @y, w: @w, h: @h)
    @callback = callback_and_args.shift
    @arguments = callback_and_args
    @enabled = enabled
    @char_disabled = "☐"
    @char_enabled = "☑"
  end

  def draw
    args = GTK.args
    inside = args.inputs.mouse.inside_rect?(@rect)
    alpha = 180
    args.outputs.primitives << @rect.merge(primitive_marker: :solid,
                                        r: 100,
                                        g: 120,
                                        b: 120,
                                        a: alpha)
    args.outputs.primitives << @rect.center.merge(text: @enabled ? @char_enabled : @char_disabled,
                                              r: 255,
                                              g: 255,
                                              b: 255,
                                              anchor_x: 0.5,
                                              anchor_y: 0.5,
                                              font: DBGUI.font)
                                              
    if inside && args.inputs.mouse.click
      trigger
    end
  end

  def trigger
    @enabled = !@enabled
    if !callback.nil?
      if arguments.nil?
        method(@callback).call
      else
        method(@callback).call(*@arguments)
      end
    end
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