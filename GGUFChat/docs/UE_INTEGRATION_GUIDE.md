# 🎮 GGUF Chat Android Library - UE 集成调用指南

## 📖 概述

本指南详细介绍如何在 Unreal Engine (UE) 中集成和使用 GGUF Chat Android Library。

### 前置条件

- ✅ Android SDK 和 NDK 已安装
- ✅ UE 项目已配置 Android 支持
- ✅ 已编译生成 Fat-AAR 文件
- ✅ 基本的 JNI 和 C++ 知识

---

## 🚀 快速开始

### 步骤 1: 编译 Fat-AAR

```bash
# 编译 UE 专用 Fat-AAR
./gradlew :llama-android:assembleUeRelease -PenableFatAar=true

# 输出文件
llama-android/build/outputs/aar/llama-android-ue-release.aar
```

### 步骤 2: 添加 AAR 到 UE 项目

```bash
# 在 UE 项目中创建目录
mkdir -p YourUEProject/Plugins/GGUFChat/Source/ThirdParty/Android/libs

# 复制 AAR 文件
cp llama-android/build/outputs/aar/llama-android-ue-release.aar \
   YourUEProject/Plugins/GGUFChat/Source/ThirdParty/Android/libs/
```

### 步骤 3: 配置 build.gradle

在 UE 项目的 `build.gradle` 中添加：

```groovy
dependencies {
    implementation(name: 'llama-android-ue-release', ext: 'aar')
}

repositories {
    flatDir {
        dirs 'src/ThirdParty/Android/libs'
    }
}
```

### 步骤 4: 添加权限

在 UE 编辑器中：
1. **Edit** → **Project Settings**
2. **Platforms** → **Android**
3. **Required Permissions** → 添加权限
   - `READ_EXTERNAL_STORAGE` (读取模型文件)
   - `WRITE_EXTERNAL_STORAGE` (可选，写入缓存)

---

## 📝 C++ 集成代码

### 方式 1: 使用 LlamaHelper（推荐）

LlamaHelper 提供了更简洁的 API，自动处理 Context 获取。

#### 1.1 初始化

```cpp
// 获取 JNI 环境
JNIEnv* Env = FAndroidApplication::GetJavaEnv();

if (Env == nullptr)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to get JNI environment"));
    return;
}

// 查找 LlamaHelper 类
jclass HelperClass = Env->FindClass("com/stdemo/ggufchat/LlamaHelper");
if (HelperClass == nullptr)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to find LlamaHelper class"));
    return;
}

// 创建实例（自动获取 Context）
jmethodID CreateMethod = Env->GetStaticMethodID(HelperClass, "create",
                                                "()Lcom/stdemo/ggufchat/LlamaHelper;");
if (CreateMethod == nullptr)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to find create method"));
    return;
}

jobject Helper = Env->CallStaticObjectMethod(HelperClass, CreateMethod);
if (Helper == nullptr)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to create LlamaHelper instance"));
    return;
}

// 初始化
jmethodID InitMethod = Env->GetMethodID(HelperClass, "initialize", "()Z");
jboolean Success = Env->CallBooleanMethod(Helper, InitMethod);

if (!Success)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to initialize LlamaHelper"));
    return;
}

UE_LOG(LogTemp, Log, TEXT("LlamaHelper initialized successfully"));
```

#### 1.2 发送消息

```cpp
// 发送消息方法
jmethodID SendMessageMethod = Env->GetMethodID(HelperClass, "sendMessage",
                                               "(Ljava/lang/String;)V");
if (SendMessageMethod == nullptr)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to find sendMessage method"));
    return;
}

// 发送消息
FString Message = TEXT("你好，请介绍一下自己");
jstring JavaMessage = Env->NewStringUTF(TCHAR_TO_UTF8(*Message));

Env->CallVoidMethod(Helper, SendMessageMethod, JavaMessage);

// 释放本地引用
Env->DeleteLocalRef(JavaMessage);

UE_LOG(LogTemp, Log, TEXT("Message sent: %s"), *Message);
```

#### 1.3 设置回调

```cpp
// 创建回调类
jclass CallbackClass = Env->FindClass("com/stdemo/ggufchat/LlamaHelper$ResponseCallback");
if (CallbackClass == nullptr)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to find ResponseCallback class"));
    return;
}

// 创建回调实例（需要实现接口）
// 这里简化处理，实际使用时需要实现 Java 接口
jmethodID SetCallbackMethod = Env->GetMethodID(HelperClass, "setOnResponse",
                                                "(Lcom/stdemo/ggufchat/LlamaHelper$ResponseCallback;)V");

// 设置回调（需要实现 Java 接口或使用反射）
// ... 详细实现见下文
```

#### 1.4 开始新对话

```cpp
jmethodID NewChatMethod = Env->GetMethodID(HelperClass, "startNewChat", "()V");
Env->CallVoidMethod(Helper, NewChatMethod);

UE_LOG(LogTemp, Log, TEXT("Started new chat"));
```

#### 1.5 释放资源

```cpp
jmethodID ReleaseMethod = Env->GetMethodID(HelperClass, "release", "()V");
Env->CallVoidMethod(Helper, ReleaseMethod);

// 释放引用
Env->DeleteLocalRef(Helper);
Env->DeleteLocalRef(HelperClass);

UE_LOG(LogTemp, Log, TEXT("LlamaHelper released"));
```

---

### 方式 2: 使用 LlamaManagerJava

更底层的 API，需要手动处理 Context。

#### 2.1 获取 Context

```cpp
// 获取 Application Context
jobject Activity = FAndroidApplication::GetGameActivity();
if (Activity == nullptr)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to get activity"));
    return;
}

JNIEnv* Env = FAndroidApplication::GetJavaEnv();

// 获取 ApplicationContext
jclass ActivityClass = Env->GetObjectClass(Activity);
jmethodID GetAppContextMethod = Env->GetMethodID(ActivityClass,
                                                  "getApplicationContext",
                                                  "()Landroid/content/Context;");
jobject AppContext = Env->CallObjectMethod(Activity, GetAppContextMethod);

if (AppContext == nullptr)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to get application context"));
    return;
}

UE_LOG(LogTemp, Log, TEXT("Got application context"));
```

#### 2.2 创建 Manager

```cpp
// 查找 LlamaManagerJava 类
jclass ManagerClass = Env->FindClass("com/stdemo/ggufchat/LlamaManagerJava");
if (ManagerClass == nullptr)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to find LlamaManagerJava class"));
    return;
}

// 获取构造方法
jmethodID Constructor = Env->GetMethodID(ManagerClass, "<init>",
                                        "(Landroid/content/Context;)V");

// 创建实例
jobject Manager = Env->NewObject(ManagerClass, Constructor, AppContext);
if (Manager == nullptr)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to create LlamaManagerJava instance"));
    return;
}

UE_LOG(LogTemp, Log, TEXT("LlamaManagerJava created"));
```

#### 2.3 初始化

```cpp
// 初始化方法
jmethodID InitMethod = Env->GetMethodID(ManagerClass, "initializeSync", "()Z");
jboolean Success = Env->CallBooleanMethod(Manager, InitMethod);

if (!Success)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to initialize LlamaManagerJava"));
    return;
}

UE_LOG(LogTemp, Log, TEXT("LlamaManagerJava initialized"));
```

#### 2.4 发送消息

```cpp
jmethodID SendMessageMethod = Env->GetMethodID(ManagerClass, "sendMessage",
                                               "(Ljava/lang/String;)V");

FString Message = TEXT("你好，请介绍一下自己");
jstring JavaMessage = Env->NewStringUTF(TCHAR_TO_UTF8(*Message));

Env->CallVoidMethod(Manager, SendMessageMethod, JavaMessage);

Env->DeleteLocalRef(JavaMessage);
```

---

## 🔄 回调处理

### 创建 Java 回调类

为了正确处理回调，需要创建一个 Java 类来实现接口：

```java
// 文件: com/stdemo/ggufchat/UECallback.java
package com.stdemo.ggufchat;

import android.util.Log;

public class UECallback implements LlamaHelper.ResponseCallback {
    private long nativePtr;

    public UECallback(long ptr) {
        this.nativePtr = ptr;
    }

    @Override
    public void onResponse(String text) {
        // 调用 native 方法
        onLlamaResponse(nativePtr, text);
    }

    @Override
    public void onError(String error) {
        // 调用 native 方法
        onLlamaError(nativePtr, error);
    }

    // Native 方法声明
    private native void onLlamaResponse(long ptr, String text);
    private native void onLlamaError(long ptr, String error);
}
```

### 实现 Native 回调

```cpp
// 在 C++ 中实现 native 方法
extern "C" JNIEXPORT void JNICALL
Java_com_stdemo_ggufchat_UECallback_onLlamaResponse(JNIEnv* Env, jobject Thiz,
                                                    jlong Ptr, jstring Text)
{
    // 转换 jstring 到 FString
    const char* Chars = Env->GetStringUTFChars(Text, nullptr);
    FString Response(UTF8_TO_TCHAR(Chars));
    Env->ReleaseStringUTFChars(Text, Chars);

    // 在游戏线程中处理
    AsyncTask(ENamedThreads::GameThread, [Response]()
    {
        // 处理响应
        UE_LOG(LogTemp, Log, TEXT("Llama Response: %s"), *Response);

        // 你可以在这里调用 Blueprint 事件或更新 UI
        // OnLlamaResponseReceived.Broadcast(Response);
    });
}

extern "C" JNIEXPORT void JNICALL
Java_com_stdemo_ggufchat_UECallback_onLlamaError(JNIEnv* Env, jobject Thiz,
                                                  jlong Ptr, jstring Error)
{
    const char* Chars = Env->GetStringUTFChars(Error, nullptr);
    FString ErrorStr(UTF8_TO_TCHAR(Chars));
    Env->ReleaseStringUTFChars(Error, Chars);

    AsyncTask(ENamedThreads::GameThread, [ErrorStr]()
    {
        UE_LOG(LogTemp, Error, TEXT("Llama Error: %s"), *ErrorStr);
        // OnLlamaErrorReceived.Broadcast(ErrorStr);
    });
}
```

### 设置回调

```cpp
// 创建回调实例
jclass UECallbackClass = Env->FindClass("com/stdemo/ggufchat/UECallback");
jmethodID UECallbackConstructor = Env->GetMethodID(UECallbackClass, "<init>", "(J)V");

jobject Callback = Env->NewObject(UECallbackClass, UECallbackConstructor,
                                 reinterpret_cast<jlong>(this));

// 设置回调
jmethodID SetCallbackMethod = Env->GetMethodID(HelperClass, "setOnResponse",
                                                "(Lcom/stdemo/ggufchat/LlamaHelper$ResponseCallback;)V");
Env->CallVoidMethod(Helper, SetCallbackMethod, Callback);
```

---

## 🔧 完整示例类

```cpp
// GGUFChatManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GGUFChatManager.generated.h"

UCLASS()
class AGGUFChatManager : public AActor
{
    GENERATED_BODY()

public:
    AGGUFChatManager();

    UFUNCTION(BlueprintCallable, Category = "GGUF Chat")
    bool Initialize();

    UFUNCTION(BlueprintCallable, Category = "GGUF Chat")
    void SendMessage(const FString& Message);

    UFUNCTION(BlueprintCallable, Category = "GGUF Chat")
    void StartNewChat();

    UFUNCTION(BlueprintCallable, Category = "GGUF Chat")
    void Release();

    // 委托声明
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResponseReceived, const FString&, Response);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnErrorReceived, const FString&, Error);

    UPROPERTY(BlueprintAssignable, Category = "GGUF Chat")
    FOnResponseReceived OnResponseReceived;

    UPROPERTY(BlueprintAssignable, Category = "GGUF Chat")
    FOnErrorReceived OnErrorReceived;

protected:
    virtual void BeginPlay() override;

private:
    jobject HelperObject;
    JNIEnv* Env;

    void SetupCallbacks();
};
```

```cpp
// GGUFChatManager.cpp
#include "GGUFChatManager.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"

AGGUFChatManager::AGGUFChatManager()
{
    HelperObject = nullptr;
    Env = nullptr;
}

void AGGUFChatManager::BeginPlay()
{
    Super::BeginPlay();
    Initialize();
}

bool AGGUFChatManager::Initialize()
{
    Env = FAndroidApplication::GetJavaEnv();
    if (Env == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to get JNI environment"));
        return false;
    }

    // 查找类并创建实例
    jclass HelperClass = Env->FindClass("com/stdemo/ggufchat/LlamaHelper");
    if (HelperClass == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find LlamaHelper class"));
        return false;
    }

    jmethodID CreateMethod = Env->GetStaticMethodID(HelperClass, "create",
                                                    "()Lcom/stdemo/ggufchat/LlamaHelper;");
    if (CreateMethod == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to find create method"));
        return false;
    }

    HelperObject = Env->CallStaticObjectMethod(HelperClass, CreateMethod);
    if (HelperObject == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create LlamaHelper instance"));
        return false;
    }

    // 初始化
    jmethodID InitMethod = Env->GetMethodID(HelperClass, "initialize", "()Z");
    jboolean Success = Env->CallBooleanMethod(HelperObject, InitMethod);

    if (!Success)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to initialize LlamaHelper"));
        return false;
    }

    // 设置回调
    SetupCallbacks();

    UE_LOG(LogTemp, Log, TEXT("GGUFChatManager initialized successfully"));
    return true;
}

void AGGUFChatManager::SendMessage(const FString& Message)
{
    if (HelperObject == nullptr || Env == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("LlamaHelper not initialized"));
        return;
    }

    jmethodID SendMessageMethod = Env->GetMethodID(
        Env->GetObjectClass(HelperObject), "sendMessage",
        "(Ljava/lang/String;)V");

    jstring JavaMessage = Env->NewStringUTF(TCHAR_TO_UTF8(*Message));
    Env->CallVoidMethod(HelperObject, SendMessageMethod, JavaMessage);
    Env->DeleteLocalRef(JavaMessage);

    UE_LOG(LogTemp, Log, TEXT("Message sent: %s"), *Message);
}

void AGGUFChatManager::StartNewChat()
{
    if (HelperObject == nullptr || Env == nullptr)
    {
        return;
    }

    jmethodID NewChatMethod = Env->GetMethodID(
        Env->GetObjectClass(HelperObject), "startNewChat", "()V");
    Env->CallVoidMethod(HelperObject, NewChatMethod);

    UE_LOG(LogTemp, Log, TEXT("Started new chat"));
}

void AGGUFChatManager::Release()
{
    if (HelperObject != nullptr && Env != nullptr)
    {
        jmethodID ReleaseMethod = Env->GetMethodID(
            Env->GetObjectClass(HelperObject), "release", "()V");
        Env->CallVoidMethod(HelperObject, ReleaseMethod);

        Env->DeleteLocalRef(HelperObject);
        HelperObject = nullptr;
    }

    UE_LOG(LogTemp, Log, TEXT("GGUFChatManager released"));
}

void AGGUFChatManager::SetupCallbacks()
{
    // 回调设置实现
    // 参考上文 "创建 Java 回调类" 部分
}
```

---

## ❓ 常见问题

### 1. ClassNotFoundException

**问题**：找不到类

**解决**：
1. 检查 AAR 文件是否正确添加
2. 检查 build.gradle 配置
3. 清理并重新构建项目

### 2. MethodNotFound

**问题**：找不到方法

**解决**：
```bash
# 使用 javap 查看正确的方法签名
unzip -j llama-android-ue-release.aar classes.jar
javap -s -p classes.jar | grep -A 5 "class LlamaHelper"
```

### 3. 回调未触发

**问题**：设置回调后没有响应

**解决**：
1. 实现 `JNI_OnLoad` 并注册 native 方法
2. 确保在正确的线程中处理回调
3. 检查 Java 回调类是否正确实现

### 4. 内存泄漏

**问题**：内存占用持续增长

**解决**：
```cpp
// 确保释放所有 JNI 引用
Env->DeleteLocalRef(HelperObject);
Env->DeleteLocalRef(HelperClass);
Env->DeleteLocalRef(Callback);
```

### 5. Context 为空

**问题**：获取 Context 失败

**解决**：
```cpp
// 检查 Activity 是否有效
jobject Activity = FAndroidApplication::GetGameActivity();
if (Activity == nullptr)
{
    UE_LOG(LogTemp, Error, TEXT("Game activity is null"));
    return false;
}
```

---

## 🔗 相关文档

- [编译构建指南](BUILD_GUIDE.md) - 如何编译 AAR 文件
- [Java API 调用指南](JAVA_API_GUIDE.md) - API 详细说明
- [项目总览](README.md) - 项目概述

---

**文档版本**: v1.0
**最后更新**: 2026-04-14
