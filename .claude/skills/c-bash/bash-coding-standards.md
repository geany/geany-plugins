# Bash coding standards

All test `.sh` scripts should follow these guidelines.

follow [bash code style](bash-code-style.md).


Presume no spaces in paths, this is Linux, no need to quote them.
Presume URLs do not have spaces, they never do, no need to quote them.
Don't quote `$port` variables
Don't use colors `${RED}` outside of the test utilities.
Avoid env vars for nodejs server parameters, use CLI args.

Make use of test utilities `test-functions.sh` and `integration-test-fucntion.sh`, do not write script specific

- assertions
- kill functions

Finish scripts with `test_summary` this will exit in a way that integrates with Makefile test targets.
For nginx integration tests just before `test_summary` run `crash_check`

## exit handlers

Handle cleanup in an exit handler, that runs in success or failure scenario.
```
cleanup() {
  cleanup_nginx
  cleanup_pid $backend_pid
  cleanup_temp_dir
}
trap cleanup EXIT
```

# Prefer standard ports

mxsrv           53880
legacy          8080
winchecker-next 9090
cache_manager   9999

# preferred vars

base_url
