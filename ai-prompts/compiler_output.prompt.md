Currently if we launch "file tools" from either tools menu or the treebrowser they run in a  tab in Cli window, if its available, or fall back to the Terminal window.

If we run `make` type commands, output would be better in the Compiler window, but this does not seem to be a proper terminal and cant handle colour.

Best solution is to support running any job we support in filetypetools.conf in a _configurable_ tab.

```ini
[pom.xml]
name_0=Mvn install
tool_0=cd %d; mvn install
tab_0=Maven
```

`tab_X=` should support...

- **_Compiler** - run comamnd without a tty, hope that makes command output not have Esc codes, and output to Compiler tab, live with shitty colour support.
- **_Messages** - run the command and print the last 1 or 2 lines of text from the command (without Esc codes) to the Message tab. Print a red or green utf8 dot when it finished according to exit code of the script.
- **_Status** - run the command and print just a red or green utf-8 dot and the command cli string, when it has finished (clour according to exit code of the script).
   Is it possible to update the tab title from "Status"  to "Status " and the coloured dot, remove the dot when new text is added.
- **_Terminal** - run in the single tab Terminal.
- **_new** - run in the Cli Tabbed UI opening a new tab for the output, set the name of the tab to the name of the tool.
- **_current** - run in the Cli Tabbed UI in the currently selected tab.
- Any string that does not start with `_` runs the command in a new tab in Cli, name of tab is the tool name, it re-uses a tab if there is one already with that name.

If tab_X is missing it should default to  "_new" behaviour.

Just discuss
