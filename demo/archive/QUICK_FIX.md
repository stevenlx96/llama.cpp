# Quick Fix Commands

Run these commands in Git Bash at `E:\MyGithub\llama.cpp\examples\llama.android`:

```bash
# 1. Verify environment variable
echo $ANDROID_HOME

# 2. Clean everything
./gradlew clean
rm -rf lib/.cxx lib/build .gradle/
cd ../.. && rm -rf build/ && cd examples/llama.android

# 3. Close and restart Android Studio

# 4. Rebuild
./gradlew :lib:assembleRelease
```

That's it! The build should now complete successfully.

See `FIX_NINJA_PATH_STEPS.md` for detailed troubleshooting if needed.
