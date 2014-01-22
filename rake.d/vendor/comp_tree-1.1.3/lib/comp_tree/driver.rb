
module CompTree
  #
  # Driver is the main interface to the computation tree.  It is
  # responsible for defining nodes and running computations.
  #
  class Driver
    #
    # See CompTree.build
    #
    def initialize(opts = {})  #:nodoc:
      @node_class = opts[:node_class] || Node
      @nodes = Hash.new
    end

    #
    # Name-to-node hash.
    #
    attr_reader :nodes

    #
    # _name_ -- unique node identifier (for example a symbol).
    #
    # _child_names_ -- unique node identifiers of children.
    #
    # Define a computation node.
    #
    # During a computation, the results of the child nodes are passed
    # to the block.  The block returns the result of this node's
    # computation.
    #
    # In the following example, a computation node named +:area+ is
    # defined which depends on the nodes named +:width+ and +:height+.
    #
    #   driver.define(:area, :width, :height) { |w, h|
    #     w*h
    #   }
    #
    def define(name, *child_names, &block)
      #
      # retrieve or create node
      #
      node = @nodes[name] ||= @node_class.new(name)
      raise RedefinitionError.new(node.name) if node.function
      node.function = block

      #
      # retrieve or create children
      #
      children = child_names.map { |child_name|
        @nodes[child_name] ||= @node_class.new(child_name)
      }

      #
      # link
      #
      node.children = children
      children.each { |child| child.parents << node }
      
      node
    end

    #
    # _name_ -- unique node identifier (for example a symbol).
    #
    # Mark this node and all its children as uncomputed.
    #
    def reset(name)
      @nodes[name].reset
    end

    #
    # _name_ -- unique node identifier (for example a symbol).
    #
    # Check for a cyclic graph below the given node.  If found,
    # returns the names of the nodes (in order) which form a loop.
    # Otherwise returns nil.
    #
    def check_circular(name)
      helper = Proc.new { |root, chain|
        if chain.include? root
          return chain + [root]
        end
        @nodes[root].children.each { |child|
          helper.call(child.name, chain + [root])
        }
      }
      helper.call(name, [])
      nil
    end

    #
    # _name_ -- unique node identifier (for example a symbol).
    #
    # _max_threads_ -- maximum number of threads, or +0+ to indicate
    # no limit.
    #
    # Compute the tree below _name_ and return the result.
    #
    # If a node's computation raises an exception, the exception will
    # be transferred to the caller of compute().  The tree will be
    # left in a dirty state so that individual nodes may be examined.
    # It is your responsibility to call reset() before attempting the
    # computation again, otherwise the result will be undefined.
    #
    def compute(name, max_threads)
      begin
        max_threads = max_threads.to_int
      rescue NoMethodError
        raise TypeError, "can't convert #{max_threads.class} into Integer"
      end
      if max_threads < 0
        raise RangeError, "max threads must be nonnegative"
      end

      root = @nodes[name] or raise NoNodeError.new(name)
      if root.computed
        root.result
      elsif max_threads == 1
        root.compute_now
      else
        Algorithm.compute_parallel(root, max_threads == 0 ? nil : max_threads)
      end
    end
  end
end
