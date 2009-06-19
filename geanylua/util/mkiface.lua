#!/usr/bin/env lua

--[[

    Script to create the "glspi_sci.h" file.
    Requires: 
      "Scintilla.h" and "Scintilla.iface" in the current directory.
      "lua" and "cpp" somewhere in your $PATH
--]]


local cmdvals={}

cpp=io.popen("cpp -dM Scintilla.h")


for line in cpp:lines()
do
  if line:match("^#define%s+SCI_") then
  table.insert(cmdvals,{
    name=line:gsub("^#define%s+([^ ]+).*", "%1"),
    value=line:gsub("^#define%s+SCI_[^%s]*%s*","")
  })
  end
end


function get_cmd_name(id)
  for i,rec in ipairs(cmdvals)
  do
    if ( rec.value == id ) then
      return rec.name:sub(5,-1)
    end
  end
  return nil
end


function translate_typename(tn)
  if (tn=="position") or (tn=="colour") or (tn=="keymod") then
    return "int"
  else
    return tn
  end
end


local alltypes={}

function add_type(tn)
  for i,v in ipairs(alltypes)
  do
    if (v==tn) then return end
  end
  table.insert(alltypes,tn)
end


local entries={}

io.input("Scintilla.iface")

for line in io.lines()
do
  if line:match("^fun%s")
    or line:match("^get%s")
      or line:match("^set%s")
  then
    rvtype=line:gsub("...%s+([^%s]+).*", "%1")
    rvtype=translate_typename(rvtype)
    arglist=line:gsub("^.*%(", "(")
    wparam=arglist:gsub("%(([^,]-),.*","%1")
    if (wparam~="") then
      wparam=wparam:gsub("^%s*","")
      wparam=wparam:gsub("%s.*$","")
      wparam=translate_typename(wparam)
    else
      wparam="void"
    end
    lparam=arglist:gsub("^[^,]*,%s*","")
    if (lparam~=")") then
      lparam=lparam:gsub("%s.*","")
      lparam=translate_typename(lparam)
    else
      lparam="void"
    end
    
    cmdid=line:gsub("^.*=([0-9]+)%(.*", "%1")
    cmdname=get_cmd_name(cmdid)
    if cmdname then 
      add_type(rvtype)
      add_type(wparam)
      add_type(lparam)
      table.insert(entries,
          "\t{\""..cmdname.."\", SLT_"..rvtype:upper()..", SCI_"..cmdname..", SLT_"..wparam:upper()..", SLT_"..lparam:upper().."},")
    end
  end
end

print(
[[

/*
 *******************  !!! IMPORTANT !!!  ***************************
 *
 * This is a machine generated file, do not edit by hand!
 * If you need to modify this file, see "geanylua/util/mkiface.lua"
 *
 *******************************************************************
 *
*/

]]
)

print("typedef enum {")
for i,v in ipairs(alltypes)
do
  print("\tSLT_"..v:upper()..", ")
end
print("\tSLT_LAST\n} GlspiType;\n\n")  


print("typedef struct _SciCmdHashEntry {")
print("\tgchar *name;")
print("\tGlspiType result;")
print("\tgint msgid;")
print("\tGlspiType wparam;")
print("\tGlspiType lparam;")
print("} SciCmdHashEntry;")
print("\n")
print("static SciCmdHashEntry sci_cmd_hash_entries[] = {")

for i,v in ipairs(entries)
do
  print(v)
end


print("\t{NULL, SLT_LAST, 0, SLT_LAST, SLT_LAST}")
print("};")


