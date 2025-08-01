sleeper = thread.new(thread.sleep, 1)
function joiner(timeout)
  print('Hello!')
  print(sleeper:join(timeout))
end

thread.new(joiner)
thread.new(joiner)
thread.sleep(0.33)
thread.new(joiner, 0.33)
