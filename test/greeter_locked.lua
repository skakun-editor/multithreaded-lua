lock = thread.newlock()
function greeter(name)
  while true do
    local lock <close> = lock:acquire()
    io.write('Hello, ', name, '!\n')
  end
end

thread.new(greeter, 'Alice')
thread.new(greeter, 'Bob')
