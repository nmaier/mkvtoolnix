
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
      :computed
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
      reset_self
      @children.each { |child|
        child.reset
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
      @children_results ||= @children.map { |child|
        unless child.computed
          return nil
        end
        child.result
      }
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

    def free?
      @lock_level.zero?
    end

    def lock
      @lock_level = @lock_level.succ
      @parents.each { |parent|
        parent.lock
      }
    end

    def unlock
      @lock_level -= 1  # Fixnum#pred not in 1.8.6
      @parents.each { |parent|
        parent.unlock
      }
    end
  end
end
