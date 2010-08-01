
require 'thread'

module CompTree
  #
  # minimal version of standard Queue
  #
  class Queue
    def initialize
      @queue = []
      @waiting = []
    end

    def push(object)
      Thread.critical = true
      @queue.push object
      if thread = @waiting.shift
        thread.wakeup
      end
    ensure
      Thread.critical = false
    end

    def pop
      while (Thread.critical = true ; @queue.empty?)
        @waiting.push Thread.current
        Thread.stop
      end
      @queue.shift
    ensure
      Thread.critical = false
    end
  end
end
