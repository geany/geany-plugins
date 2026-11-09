# geanyvosk - pluging to control geany with voice

The goal of this project is two fold.

First to enable controlling the ui with voice triggered by a specific Wake Work "Rub lamp"
After the wake work I should be able to 
- voice commands from the main menu
- switch tabs (e.g switch to Agent tab) and gain focus there
- switch to cli tab, select a terminal tab by name and get focus
- shold use our custom geany control plugin


Secondly I would like to be able to create new claude skills and execute skills by voice
- "agent create new skill ..." , give it a name, and then open the editor and speak text tata gets written to the SKILL.md file
- I would also like to be able to open a new ./ai-prompt Markdown file and speak text into it, e.g. "new ai prompt my project dot md"
- I would like to be able to run agent skills with specific context files, 
	e.g. "Agent Skill opus plan, context my project dot md, go"  
		"Agent skill opus exec context my project dot md, go"
	It should not start untill I say Go

Saying the words "Back in the lamp" should exit voice activation mode.  

We should prefer C libraries for the voice control e.g. PocketSphinx and Vosk
