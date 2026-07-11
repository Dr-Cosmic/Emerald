# shellexec captures standard output (echo is portable across platforms)
string result = shellexec("echo emerald-shell-test");
print(result);
print(len(result) > 0);
