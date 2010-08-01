
module CompTree
  #
  # Base class for nodes in the computation tree.
  # 
  class Node
    attr_reader :name

    attr_accessor(
      :parents,
      :children,
      :function,
      :result,
      :computed,
      :lock_level
    )

    #
    # Create a node
    #
    def initialize(name)
      @name = name
      @parents = []
      @children = []
      @function = nil
      reset_self
    end

    #
    # Reset the computation for this node.
    #
    def reset_self
      @result = nil
      @computed = nil
      @lock_level = 0
      @children_results = nil
    end

    #
    # Reset the computation for this node and all children.
    #
    def reset
      each_downward { |node|
        node.reset_self
      }
    end

    def each_downward(&block)
      block.call(self)
      @children.each { |child|
        child.each_downward(&block)
      }
    end

    def each_upward(&block)
      block.call(self)
      @parents.each { |parent|
        parent.each_upward(&block)
      }
    end

    def each_child
      @children.each { |child|
        yield(child)
      }
    end

    #
    # Force all children and self to be computed; no locking required.
    # Intended to be used outside of parallel computations.
    #
    def compute_now
      unless @computed
        unless @children_results
          @children_results = @children.map { |child|
            child.compute_now
          }
        end
        compute
        if @computed.is_a? Exception
          raise @computed
        end
      end
      @result
    end
    
    #
    # If all children have been computed, return their results;
    # otherwise return nil.
    #
    def children_results
      @children_results or (
        @children_results = @children.map { |child|
          unless child.computed
            return nil
          end
          child.result
        }
      )
    end

    #
    # Compute this node; children must be computed and lock must be
    # already acquired.
    #
    def compute
      begin
        unless @function
          raise NoFunctionError.new(@name)
        end
        @result = @function.call(*@children_results)
        @computed = true
      rescue Exception => exception
        @computed = exception
      end
      @result
    end

    def locked?
      @lock_level != 0
    end

    def lock
      each_upward { |node|
        node.lock_level += 1
      }
    end

    def unlock
      each_upward { |node|
        node.lock_level -= 1
      }
    end
  end
end
