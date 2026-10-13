---
name: mdalign
description: Align tables in markdown files
---

user will indicate which file needs tables adjusted, if skill is run with no arguments align the last created md file

<objective>
Ensure the text representation of tables, is easy to read wihtout rendering the markdown as HTML
</objective>

<process>
Adjust the whitespace and `|----|` header markers or all tables in the file to fixed width of the longest text in a cell in the tables column.

N.B. headers should have no whitespace characters

e.g

```
| #   | Task                                                             | Classes            | Est. Lines | Est. Gain         |
|-----|------------------------------------------------------------------|--------------------|------------|-------------------|
```

Use the .claude/skills/mdalign/mdalign.py script to achieve this, then check it worked
</process>
