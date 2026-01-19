class ImageButton
  attr_accessor :callback, :arguments

  def initialize(x, y, w, h, path, *callback_and_args)
    @x = x
    @y = y
    @w = w
    @h = h
    @rect = GTK.args.layout.rect(col: @x, row: @y, w: @w, h: @h)
    @image = GTK.args.layout.rect(col: x, row: y, w: w, h: h)
    @path = path
    @pixels = GTK.args.gtk.get_pixels(path)
    @callback = callback_and_args.shift
    @arguments = callback_and_args

    calculate_img_size
  end

  def draw
    args = GTK.args
    inside = args.inputs.mouse.inside_rect?(@rect)
    alpha = inside ? 200 : 160

    args.outputs.primitives << @rect.merge(primitive_marker: :solid,
                                        r: 100,
                                        g: 100,
                                        b: 160,
                                        a: alpha)
    args.outputs.primitives << @rect.merge(primitive_marker: :border,
                                        r: 255,
                                        g: 255,
                                        b: 255)
     args.outputs.primitives << @image.merge(path: @path,
                                        r: 255,
                                        g: 255,
                                        b: 255)


    if inside && args.inputs.mouse.click
      trigger
    end
  end

  def calculate_img_size
rect_w = @rect.w
  rect_h = @rect.h
  img_w  = @pixels.w
  img_h  = @pixels.h

  scale    = [rect_w / img_w, rect_h / img_h].min
  # -10 is to prevent overlapping with border
  target_w = (img_w * scale) - 10
  target_h = (img_h * scale) - 10

  x = @rect.x + (rect_w - target_w) / 2
  y = @rect.y + (rect_h - target_h) / 2

  @image = { x: x, y: y, w: target_w, h: target_h }
  end

  def trigger
    if !callback.nil?
      if arguments.nil?
        method(@callback).call
      else
        method(@callback).call(*@arguments)
      end
    end
  end

  def path
    @path
  end

  def path=(path)
    @path = path
    calculate_img_size
  end

  def x
    @x
  end

  def x=(x)
    @x = x
    @rect = GTK.args.layout.rect(col: @x, row: @y, w: @w, h: @h)
    calculate_img_size
  end

  def y
    @y
  end

  def y=(y)
    @y = y
    @rect = GTK.args.layout.rect(col: @x, row: @y, w: @w, h: @h)
    calculate_img_size
  end

  def w
    @rect.w
  end

  def w=(w)
    @w = w
    @rect = GTK.args.layout.rect(col: @x, row: @y, w: @w, h: @h)
    calculate_img_size
  end

  def h
    @h
  end

  def h=(h)
    @h = h
    @rect = GTK.args.layout.rect(col: @x, row: @y, w: @w, h: @h)
    calculate_img_size
  end
end