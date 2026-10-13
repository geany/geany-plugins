Geany progress works, but it needs more funnctions...

When starting a new progress flow we should send it the plan the progresss is for, so the GUI can access it.

the progress & plan should be paired with good naming conventions

.planing/plans/xxxx_PLAN.md
.planing/state/xxxx_PROGRESS.md

We need to update the skill   opus-plan and opus-exec to use these new standard file locations.

Clicking on the title should then open the progress .md in the editor.
Right click should have a popup with some options
- mark finished to manually finish a task
- open the markdown version of the progress.
- open the plan (same as clicking the title)
- load (should open the markdown progress .md version in the GUI, it should ask for the  plan, if it cant find it automatically in .planing/plans/xxxx_PLAN.md)

The icons are black and white, use utf-8 green tick for complete


Also we want the agent to be able to add particularly relevant "for review" files and line number, when completing the task. 
When completing a phase the Agent can then recommend some files that need
review, it does not to be all files that change , for example it might request reivew of C code but mot make files and script changes.
When clicking on completed phase the plugin should open the  relevant files and scroll to the relevant line numbers.
It should also support addin relevant alerts or warnings that should show when hovering the finished phase.

