# Agent plan progress 

We need a new UI component that allows tools, specifically AI agents to report status of progress on plans being executed.

The UI should show a lpane name and list of numbered, named progress steps and a completion flag.

Currently Claude writes a text file, but this does not auto update as progress happens.

## Status

| Phase | Description                          | Status   |
|-------|--------------------------------------|----------|
| 1     | Project scaffold & build system      | complete |
| 2     | ALSA microphone capture thread       | complete |
| 3     | Vosk ASR + wake word detection       | complete |
| 4     | UI command dispatch via geanycontrol | complete |
| 5     | Skill & AI-prompt creation by voice  | complete 
| 6     | Agent skill execution by voice       | complete |

We need a plugin with a similar view that updates as Claude finished phases this should be built into the panel that hosts the treebrowser plugin.
In order to setup a new plan, Claude agent must send then name and list of phases to an API, this call will be specific to the Geany instance in which Claude is running.
As each phase  is completed Claude should call an api to update the progress.
The plugin should support writing the data to a .md file in `./planning/state/xxx.md` as well so that the progress can go into version control if needed.

The complicated part is getting the running instance of an AI agent in the Geany GUI to connect to the correct instance of the API.nce the agent is simly loaded as a CLI app.





