# =============================================================================
# llama-android library - ProGuard rules
# =============================================================================
# These rules apply when this library itself is minified (release buildType).
# minifyEnabled is currently false, so this file is only referenced — not run.
# Kept as a placeholder so build.gradle's `proguardFiles` reference resolves.
# =============================================================================

# Keep all classes in the public API surface package so reflection works.
-keep class com.stdemo.ggufchat.** { *; }

# Keep JNI native method names (matched by JNI at runtime).
-keepclasseswithmembernames class * {
    native <methods>;
}
