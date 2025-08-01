thread.new(function()
  thread.new(thread.sleep, 1):join()
end)
