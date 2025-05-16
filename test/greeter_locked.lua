lock = thread.newlock()
function greeter(name)
  while true do
    lock:acquire()
    io.write('Hello, ', name, '!\n')
    lock:release()
  end
end

thread.new(greeter, 'Alice')
thread.new(greeter, 'Bob')
