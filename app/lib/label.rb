class Label
  attr_accessor :text

  def initialize(x, y, w, h, text, len)
    @x = x
    @y = y
    @w = w
    @h = h
    @rect = GTK.args.layout.rect(col: @x, row: @y, w: @w, h: @h)
    @text = text
  end

  def draw
    args = GTK.args
    alpha = 180
    args.outputs.primitives << @rect.merge(primitive_marker: :solid,
                                        r: 100,
                                        g: 100,
                                        b: 100,
                                        a: alpha)
    args.outputs.primitives << @rect.center.merge(text: @text,
                                              r: 255,
                                              g: 255,
                                              b: 255,
                                              anchor_x: 0.5,
                                              anchor_y: 0.5,
                                              font: DBGUI.font)
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