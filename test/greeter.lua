function greeter(name)
  while true do
    io.write('Hello, ', name, '!\n')
  end
end

thread.new(greeter, 'Alice')
thread.new(greeter, 'Bob')
