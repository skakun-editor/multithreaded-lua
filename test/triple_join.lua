sleeper = thread.new(os.execute, 'sleep 1')
function joiner(timeout)
  print('Hello!')
  print(sleeper:join(timeout))
end

thread.new(joiner)
thread.new(joiner)
os.execute('sleep 0.33')
thread.new(joiner, 0.33)
