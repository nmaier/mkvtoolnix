
module CompTree
  module Algorithm
    module_function

    def compute_parallel(root, num_threads)
      from_workers = Queue.new
      to_workers = Queue.new

      workers = (1..num_threads).map {
        Thread.new {
          worker_loop(from_workers, to_workers)
        }
      }

      node = master_loop(root, num_threads, from_workers, to_workers)

      num_threads.times { to_workers.push(nil) }
      workers.each { |t| t.join }
      
      if node.computed.is_a? Exception
        raise node.computed
      else
        node.result
      end
    end

    def worker_loop(from_workers, to_workers)
      while node = to_workers.pop
        node.compute
        from_workers.push(node)
      end
    end

    def master_loop(root, num_threads, from_workers, to_workers)
      num_working = 0
      node = nil
      while true
        if num_working == num_threads or !(node = find_node(root))
          #
          # maxed out or no nodes available -- wait for results
          #
          node = from_workers.pop
          node.unlock
          num_working -= 1
          if node == root or node.computed.is_a? Exception
            break node
          end
        else
          #
          # found a node
          #
          num_working += 1
          node.lock
          to_workers.push(node)
        end
      end
    end

    def find_node(node)
      if node.computed
        #
        # already computed
        #
        nil
      elsif not node.locked? and node.children_results
        #
        # Node is not computed, not locked, and its children are
        # computed; ready to compute.
        #
        node
      else
        #
        # locked or children not computed; recurse to children
        #
        node.each_child { |child|
          if found = find_node(child)
            return found
          end
        }
        nil
      end
    end
  end
end
