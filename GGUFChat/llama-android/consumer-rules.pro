# =============================================================================
# llama-android library - Consumer ProGuard rules
# =============================================================================
# These rules are packaged into the AAR and applied to the CONSUMER app
# (demo/UE integration) when it minifies its own release build.
#
# Any rule required to keep this library's public API working under R8/ProGuard
# in consumer apps must live here.
# =============================================================================

# Public Kotlin API — keep classes, constructors, methods, fields.
# Consumers use these via reflection-like patterns (typealias, callbacks).
-keep class com.stdemo.ggufchat.LlamaEngine { *; }
-keep class com.stdemo.ggufchat.LlamaEngine$* { *; }
-keep class com.stdemo.ggufchat.LlamaHelper { *; }
-keep class com.stdemo.ggufchat.LlamaManagerJava { *; }
-keep class com.stdemo.ggufchat.ModelDownloader { *; }
-keep class com.stdemo.ggufchat.ModelManager { *; }
-keep class com.stdemo.ggufchat.ModelConfig { *; }
-keep class com.stdemo.ggufchat.Message { *; }
-keep class com.stdemo.ggufchat.ChatPromptBuilder { *; }
-keep class com.stdemo.ggufchat.PersistentStorageHelper { *; }

# Intent recognition (optional feature — ONNX runtime)
-keep class com.stdemo.ggufchat.IntentRecognizer { *; }
-keep class com.stdemo.ggufchat.IntentRecognizerManager { *; }
-keep class com.stdemo.ggufchat.IntentConfig { *; }
-keep class com.stdemo.ggufchat.IntentDiagnostic { *; }
-keep class com.stdemo.ggufchat.IntentModelManager { *; }
-keep class com.stdemo.ggufchat.IntentResult { *; }
-keep class com.stdemo.ggufchat.IntentSlot { *; }

# JNI native methods — names must be preserved for JNI lookup
-keepclasseswithmembernames class com.stdemo.ggufchat.** {
    native <methods>;
}

# Kotlin metadata for typealias (GGUFChatEngine = LlamaEngine)
-keepattributes *Annotation*, Signature, InnerClasses, EnclosingMethod
