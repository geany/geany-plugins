#!/usr/bin/env lua

--[[
    Script to create the "glspi_keycmd.h" file.

    Requires: 
      ../geany/src/keybindings.h relative to the current directory.
      "lua" and "cpp" somewhere in your $PATH

    Note that you should invoke this script from the top-level /geanylua/
    directory, not from /geanylua/util/

    The only time you need this is if the keybinding group-IDs or key-IDs
    in /geany/src/keybindings.h have changed. But it is certainly possible
    for that file to have been altered so radically that this script will
    fail anyway!

--]]




local names={}

local cpp=io.popen(
  "cpp -P `pkg-config --cflags gtk+-2.0` -I../geany/src util/keydummy.h"
)

for line in cpp:lines()
do
  if line:find("^[ \t]*GEANY_KEYS_") and not line:find("^[ \t]*GEANY_KEYS_GROUP")
  then
    name=line:gsub("^[ \t]*GEANY_KEYS_([^ \t,]+).*$", "%1")
    table.insert(names, name)
  end
end


print(
[[

/*
 *******************  !!! IMPORTANT !!!  ***************************
 *
 * This is a machine generated file, do not edit by hand!
 * If you need to modify this file, see "geanylua/util/mk-keytab.lua"
 *
 *******************************************************************
 *
 */

]]
)


print("typedef struct _KeyCmdHashEntry {")
print("\tgchar *name;")
print("\tguint group;")
print("\tguint key_id;")
print("} KeyCmdHashEntry;")
print("\n")

print("static KeyCmdHashEntry key_cmd_hash_entries[] = {")

for num,name in pairs(names)
do
  print(
   string.format("\t{\"%s\", GEANY_KEY_GROUP_%s, GEANY_KEYS_%s},",
      name, name:gsub("_.*", ""), name)
   )
end

print("\t{NULL, 0, 0}")
print("};")





