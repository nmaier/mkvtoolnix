# 
# Copyright (c) 2008-2011 James M. Lawrence.  All rights reserved.
# 
# Permission is hereby granted, free of charge, to any person
# obtaining a copy of this software and associated documentation files
# (the "Software"), to deal in the Software without restriction,
# including without limitation the rights to use, copy, modify, merge,
# publish, distribute, sublicense, and/or sell copies of the Software,
# and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
# 

require 'comp_tree/error'
require 'comp_tree/queue/queue'
require 'comp_tree/node'
require 'comp_tree/algorithm'
require 'comp_tree/driver'

#
# CompTree -- Parallel Computation Tree.
#
# See README.rdoc.
#
module CompTree
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
  #     # Compute the area using up to four parallel threads.
  #     puts driver.compute(:area, 4)
  #     # => 63
  # 
  #     # We've done this computation.
  #     puts((7 + 2)*(5 + 2))
  #     # => 63
  #   end
  #
  # A custom <code>CompTree::Node</code> subclass may optionally be
  # provided,
  #
  #   CompTree.build(:node_class => MyNode) { ... }
  #
  # This will build the tree with +MyNode+ instances.
  #
  def self.build(opts = {})
    yield Driver.new(opts)
  end
end
