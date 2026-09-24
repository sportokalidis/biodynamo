Ubuntu validation

`Ubuntu CI` and `Ubuntu System CI` cover Ubuntu 22.04, 24.04 and 26.04.
Both run on pushes, pull requests to master, and manual dispatch; system CI also
runs nightly. Matrix jobs do not cancel each other when one platform fails.

Ubuntu 26.04 currently tests the Ubuntu 24.04 binary archives, built with the
platform's default compiler (gcc-11 is installed only so ROOT's Cling
interpreter finds matching headers), Python 3.9.25 and CMake 3.31.10. Its
prerequisite installer is shared with local installation. System CI installs
the pushed branch via the public installer, then tests the installed
distribution as well as the build-tree demos and Valgrind targets. This also
works for fork branches.

After pushing the branch, check **both workflows** in the fork's Actions tab.
Check dependency installation, configure/build, unit tests, installed simulation,
demos and Valgrind, especially the Ubuntu 26.04 jobs. A green build alone does
not establish runtime compatibility of the reused third-party libraries.

Debug github actions.

Add the following step before the failing step.
ssh into the tmate session immediately. 
Otherwise the session times out and doesn't accept any input.

```
    - name: Setup tmate session
      uses: mxschmitt/action-tmate@v2
```

