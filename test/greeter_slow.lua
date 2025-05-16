function greeter(name)
  while true do
    io.write('Hello, ', name, '!\n')
    os.execute('sleep 0.1')
  end
end

thread.new(greeter, 'Alice')
thread.new(greeter, 'Bob')
