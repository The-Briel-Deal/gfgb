vim.api.nvim_create_autocmd({ "BufEnter", "BufWinEnter" }, {
	pattern = { "*.h" },
	callback = function(_)
		vim.o.filetype = "cpp"
	end,
})
