
module CompTree
  #
  # Base class for CompTree errors.
  #
  class Error < StandardError
  end

  #
  # Base class for node errors.
  #
  class NodeError < Error
    def initialize(node_name)  #:nodoc:
      @node_name = node_name
      super(custom_message)
    end
    attr_reader :node_name
  end
    
  #
  # An attempt was made to redefine a node.
  #
  # If you wish to only replace the function, set
  #   driver.nodes[name].function = lambda { ... }
  #
  class RedefinitionError < NodeError
    def custom_message  #:nodoc:
      "attempt to redefine node `#{node_name.inspect}'"
    end
  end
  
  #
  # Encountered a node without a function during a computation.
  #
  class NoFunctionError < NodeError
    def custom_message  #:nodoc:
      "no function was defined for node `#{node_name.inspect}'"
    end
  end

  #
  # Requested node does not exist.
  #
  class NoNodeError < NodeError
    def custom_message  #:nodoc:
      "no node named `#{node_name.inspect}'"
    end
  end
end
