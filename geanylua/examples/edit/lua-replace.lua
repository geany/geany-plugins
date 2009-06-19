--[[
  Replace text using Lua pattern syntax
--]]

function esc(s)
  s=s and s:gsub('"', '\\"') or ""
  assert(loadstring('rv="'..s..'"'))()
  return rv
end


if geany.count() > 0 then

  local dlg = dialog.new("Lua replace", {"Replace All", "Cancel"})

  dlg:heading(
    "          Replaces text using Lua pattern matching syntax.          ")
  dlg:hr()
  dlg:text("find", nil, "Old needle: ")
  dlg:text("repl", nil, "New needle: ")

  if (#geany.selection()>0) then
    dlg:group("scope", "sel", "Haystack:")
    dlg:radio("scope", "doc", "Document")
    dlg:radio("scope", "sel", "Selection")
  end

  local btn, results = dlg:run()

  if (btn==1) then
    if results.find then
      local func = (results.scope=="sel") and geany.selection or geany.text
      func(string.gsub(func(), esc(results.find), esc(results.repl) ))
    end
  end

else
  geany.message("No document!")
end

