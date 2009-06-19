--[[
  Simple calculator to evaluate selected text as a mathematical expression.
  Only single-line constant expressions are supported, but you can do
  some fairly complex calculations, e.g.  sqrt(pi^2*sin(rad(45)))
--]]


-- Copy math functions to global namespace
for k,v in pairs(math) do _G[k] = math[k] end


-- Set window caption for some error messages
geany.banner="Calculator"


-- Grab any selected text
sel=geany.selection()


-- Do we have a selection ?
if (sel==nil) or (#sel==0) then
  geany.message("Nothing selected.",
    "Select a math expression in the\ncurrent document, and try again.")
  return
end


-- Is our selection all on one line ?
if string.find(sel, "\n") then
  geany.message("Too many lines!",
    "Only single-line expressions are supported.")
  return
end


-- Create a function call around the selected expression
func=assert(loadstring("return "..geany.selection()))


-- Did we get our function ?
if not func then
  geany.message("Failed to parse expression.")
  return
end


-- Call the function
answer=func() 


-- Did we get an answer ?
if not answer then
  geany.message("Failed to evaluate expression.")
  return
end


-- Try to convert answer to a string
answer=tostring(answer)


-- Is the answer now a string?
if not answer then
  geany.message("Failed to convert results to text.")
  return
end


-- Replace current selection with our answer
geany.selection(answer)


