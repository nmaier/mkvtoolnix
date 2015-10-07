
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
      thread = @waiting.shift and thread.wakeup
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
