
#
# CompTree -- Parallel Computation Tree.
#
# See README.rdoc.
#
module CompTree
  VERSION = "1.0.1"

  class << self
    #
    # :call-seq:
    #   build { |driver| ... }
    #
    # Build a new computation tree. A Driver instance is passed to the
    # given block.
    #
    # Example:
    #   CompTree.build do |driver|
    # 
    #     # Define a function named 'area' taking these two arguments.
    #     driver.define(:area, :width, :height) { |width, height|
    #       width*height
    #     }
    #     
    #     # Define a function 'width' which takes a 'border' argument.
    #     driver.define(:width, :border) { |border|
    #       7 + border
    #     }
    #     
    #     # Ditto for 'height'.
    #     driver.define(:height, :border) { |border|
    #       5 + border
    #     }
    # 
    #     #    
    #     # Define a constant function 'border'.
    #     driver.define(:border) {
    #       2
    #     }
    #     
    #     # Compute the area using four parallel threads.
    #     puts driver.compute(:area, 4)
    #     # => 63
    # 
    #     # We've done this computation.
    #     puts((7 + 2)*(5 + 2))
    #     # => 63
    #   end
    #
    def build(opts = {})
      yield Driver.new(opts)
    end
  end
end
