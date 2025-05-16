thread.new(function()
  thread.new(os.execute, 'sleep 1'):join()
end)
