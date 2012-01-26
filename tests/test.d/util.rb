$message_mutex = Mutex.new
def show_message message
  $message_mutex.lock
  puts message
  $message_mutex.unlock
end

def error_and_exit text, exit_code = 2
  show_message text
  exit exit_code
end

