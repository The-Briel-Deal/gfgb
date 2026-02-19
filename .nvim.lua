 vim.api.nvim_create_autocmd({'BufEnter', 'BufWinEnter'}, {
   pattern = {'*.h'},
   callback = function(ev)
     vim.o.filetype = 'cpp'
     print(string.format('event fired: %s', vim.inspect(ev)))
   end
 })
