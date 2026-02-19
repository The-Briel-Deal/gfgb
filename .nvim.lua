 vim.api.nvim_create_autocmd({'BufEnter', 'BufWinEnter'}, {
   pattern = {'*.h'},
   callback = function(ev)
     vim.o.filetype = 'cpp'
   end
 })
