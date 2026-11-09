We should be able to open an existing project with voice.

This needs geanycontrol to have a feature then geanvosk to call it.

It requires fuzzy file matching feature

'open project <name>"  should do two thing first look in the menu for recent projects that match the file <name>.geany.

then look in `*_workspace` for `<name>.geany` files
then use `locate name.geany`
