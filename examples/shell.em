# shell.em - Running shell commands, like subprocess in Python.

string files = shellexec("echo one two three");
print("The command said: {files}");

# shellexec returns the command's standard output as a string,
# so it composes with the rest of the language:
print("It said {len(files)} characters worth of things.");
