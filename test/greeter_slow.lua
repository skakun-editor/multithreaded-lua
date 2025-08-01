function greeter(name)
  while true do
    io.write('Hello, ', name, '!\n')
    thread.sleep(0.1)
  end
end

thread.new(greeter, 'Alice')
thread.new(greeter, 'Bob')
