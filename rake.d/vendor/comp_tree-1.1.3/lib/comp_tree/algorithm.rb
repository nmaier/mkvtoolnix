
module CompTree
  module Algorithm
    module_function

    def compute_parallel(root, max_threads)
      workers = []
      from_workers = Queue.new
      to_workers = Queue.new

      node = master_loop(root, max_threads, workers, from_workers, to_workers)

      workers.size.times { to_workers.push(nil) }
      workers.each { |t| t.join }
      
      if node.computed.is_a? Exception
        raise node.computed
      end
      node.result
    end

    def new_worker(from_workers, to_workers)
      Thread.new {
        while node = to_workers.pop
          node.compute
          from_workers.push(node)
        end
      }
    end

    def master_loop(root, max_threads, workers, from_workers, to_workers)
      num_working = 0
      node = nil
      while true
        if num_working == max_threads or !(node = find_node(root))
          #
          # maxed out or no nodes available -- wait for results
          #
          node = from_workers.pop
          node.unlock
          num_working -= 1
          if node == root or node.computed.is_a? Exception
            return node
          end
        else
          #
          # not maxed out and found a node -- compute it
          #
          if (!max_threads.nil? and workers.size < max_threads) or
              (max_threads.nil? and num_working == workers.size)
            workers << new_worker(from_workers, to_workers)
          end
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
      elsif node.free? and node.children_results
        #
        # Node is not computed, not locked, and its children are
        # computed; ready to compute.
        #
        node
      else
        #
        # locked or children not computed; recurse to children
        #
        node.children.each { |child|
          found = find_node(child) and return found
        }
        nil
      end
    end
  end
end
