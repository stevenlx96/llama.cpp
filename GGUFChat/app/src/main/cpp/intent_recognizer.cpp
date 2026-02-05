/**
 * ============================================================================
 * intent_recognizer.cpp - Intent 意图识别核心实现
 * ============================================================================
 *
 * 【文件作用】
 * 这是 Intent 意图识别的核心实现文件，使用 ONNX Runtime 进行推理。
 *
 * 【技术背景】
 * 1. ONNX (Open Neural Network Exchange): 开放神经网络交换格式
 *    - 允许在不同框架之间迁移模型 (PyTorch → ONNX → 移动端)
 *    - ONNX Runtime: 微软开发的高性能推理引擎
 *
 * 2. 意图识别模型架构 (Joint Model):
 *    - 基于 BERT 类的 Transformer 模型
 *    - 同时输出: Intent (意图分类) + Slot (槽位标注)
 *
 * 【模型输入输出】
 *
 *   输入文本: "播放周杰伦的歌"
 *         │
 *         ▼
 *   ┌─────────────────────────────────┐
 *   │  分词 (Tokenization)             │
 *   │  [CLS] 播 放 周 杰 伦 的 歌 [SEP] │
 *   └─────────────────────────────────┘
 *         │
 *         ▼
 *   ┌─────────────────────────────────┐
 *   │  ONNX 模型推理                   │
 *   └─────────────────────────────────┘
 *         │
 *         ├─────────────────┐
 *         ▼                 ▼
 *   ┌───────────┐    ┌───────────────┐
 *   │ Intent    │    │ Slot Tags     │
 *   │ play_music│    │ O O B-artist  │
 *   │ (95%)     │    │ I-artist I-..│
 *   └───────────┘    └───────────────┘
 *
 * 【BIO 标注法】
 * - B-xxx: Begin，实体开始
 * - I-xxx: Inside，实体内部
 * - O: Outside，非实体
 *
 * 例如 "周杰伦" 的标注: B-artist, I-artist, I-artist
 *
 * ============================================================================
 */

// ============================================================================
// 头文件引入
// ============================================================================

#include "intent_recognizer.h"    // 头文件 (类定义)
#include <onnxruntime_cxx_api.h>  // ONNX Runtime C++ API
#include <fstream>                // 文件流
#include <sstream>                // 字符串流
#include <iostream>               // 标准输入输出
#include <algorithm>              // 算法 (std::max_element 等)
#include <cmath>                  // 数学函数 (std::exp 等)
#include <codecvt>                // 编码转换
#include <locale>                 // 本地化
#include <android/log.h>          // Android 日志

// JSON 解析库 (nlohmann/json 是一个非常流行的 C++ JSON 库)
#include <nlohmann/json.hpp>

// ============================================================================
// 日志宏定义
// ============================================================================

#define INTENT_TAG "IntentRecognizerCPP"

#define LOG_I(...) __android_log_print(ANDROID_LOG_INFO, INTENT_TAG, __VA_ARGS__)
#define LOG_E(...) __android_log_print(ANDROID_LOG_ERROR, INTENT_TAG, __VA_ARGS__)
#define LOG_W(...) __android_log_print(ANDROID_LOG_WARN, INTENT_TAG, __VA_ARGS__)

// ============================================================================
// intent 命名空间
// ============================================================================
// 命名空间用于避免函数名冲突，类似于 Python 的模块

namespace intent {

// ============================================================================
// UTF-8 工具函数
// ============================================================================
/**
 * 【函数】将 UTF-8 字符串按字符拆分
 *
 * 【为什么需要这个？】
 * 中文在 UTF-8 中每个字符占 3 个字节，不能简单地按字节拆分。
 * 这个函数正确处理多字节字符。
 *
 * 【示例】
 * 输入: "你好"
 * 输出: ["你", "好"]
 *
 * @param utf8_text - UTF-8 编码的文本
 * @return 字符列表
 */
std::vector<std::string> utf8_split_chars(const std::string& utf8_text) {
    std::vector<std::string> chars;
    size_t i = 0;

    while (i < utf8_text.length()) {
        size_t char_len = 1;
        unsigned char c = utf8_text[i];

        // 根据 UTF-8 编码规则判断字符长度
        // UTF-8 编码规则:
        // - 0xxxxxxx: 1 字节 (ASCII)
        // - 110xxxxx 10xxxxxx: 2 字节
        // - 1110xxxx 10xxxxxx 10xxxxxx: 3 字节 (中文)
        // - 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx: 4 字节 (Emoji)
        if ((c & 0x80) == 0) {          // 0xxxxxxx - ASCII
            char_len = 1;
        } else if ((c & 0xE0) == 0xC0) { // 110xxxxx - 2 bytes
            char_len = 2;
        } else if ((c & 0xF0) == 0xE0) { // 1110xxxx - 3 bytes
            char_len = 3;
        } else if ((c & 0xF8) == 0xF0) { // 11110xxx - 4 bytes
            char_len = 4;
        }

        // 检查是否有足够的字节
        if (i + char_len <= utf8_text.length()) {
            std::string char_str = utf8_text.substr(i, char_len);

            // 跳过空白字符
            if (char_str != " " && char_str != "\t" && char_str != "\n" && char_str != "\r") {
                chars.push_back(char_str);
            }
            i += char_len;
        } else {
            break;
        }
    }
    return chars;
}

// ============================================================================
// PIMPL 模式实现类
// ============================================================================
/**
 * 【设计模式】PIMPL (Pointer to Implementation)
 *
 * PIMPL 是 C++ 中常用的设计模式，用于隐藏实现细节。
 * - 头文件只暴露接口，不暴露具体实现
 * - 减少编译依赖，加快编译速度
 *
 * 【类比 Python】
 * 类似于 Python 中使用下划线前缀的私有变量:
 * class IntentRecognizer:
 *     def __init__(self):
 *         self._impl = _IntentRecognizerImpl()
 */
class IntentRecognizer::Impl {
public:
    // ONNX Runtime 相关对象
    Ort::Env env;                              // ONNX Runtime 环境
    Ort::Session session{nullptr};             // ONNX 推理会话
    Ort::AllocatorWithDefaultOptions allocator; // 内存分配器

    // 输入输出信息
    std::vector<const char*> input_names;      // 输入节点名称
    std::vector<const char*> output_names;     // 输出节点名称
    std::vector<std::vector<int64_t>> input_shapes;   // 输入形状
    std::vector<std::vector<int64_t>> output_shapes;  // 输出形状

    // 词汇表 (token -> id 的映射)
    // 例如: "你" -> 1234, "好" -> 5678
    std::map<std::string, int64_t> vocab;

    // 特殊 token 的 ID
    int64_t pad_token_id = 0;      // [PAD]: 填充 token
    int64_t cls_token_id = 101;    // [CLS]: 句子开始
    int64_t sep_token_id = 102;    // [SEP]: 句子结束
    int64_t unk_token_id = 100;    // [UNK]: 未知 token

    /**
     * 【构造函数】
     * 初始化 ONNX Runtime 环境
     */
    Impl() : env(ORT_LOGGING_LEVEL_WARNING, "IntentRecognizer") {}

    /**
     * 【析构函数】
     * 释放动态分配的内存
     */
    ~Impl() {
        // 释放 input_names 和 output_names 中的字符串
        for (auto name : input_names) delete[] name;
        for (auto name : output_names) delete[] name;
    }
};

// ============================================================================
// IntentRecognizer 构造函数和析构函数
// ============================================================================

/**
 * 【构造函数】
 * @param config - 配置参数
 */
IntentRecognizer::IntentRecognizer(const IntentConfig& config)
    : config_(config), impl_(std::make_unique<Impl>()) {
    // std::make_unique 创建 unique_ptr，自动管理内存
}

/**
 * 【析构函数】
 * = default 表示使用编译器生成的默认实现
 */
IntentRecognizer::~IntentRecognizer() = default;

// ============================================================================
// 加载标签文件
// ============================================================================
/**
 * 【函数】从文件加载标签列表
 *
 * 标签文件格式 (每行一个标签):
 * play_music
 * stop_music
 * set_volume
 * ...
 *
 * @param filename - 文件名 (相对于 model_dir)
 * @param labels   - 输出的标签列表
 * @return 是否成功
 */
bool IntentRecognizer::load_labels(const std::string& filename, std::vector<std::string>& labels) {
    std::string filepath = config_.model_dir + "/" + filename;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        LOG_E("Failed to open label file: %s", filepath.c_str());
        return false;
    }

    labels.clear();
    std::string line;
    while (std::getline(file, line)) {
        // 去除首尾空白
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (!line.empty()) {
            labels.push_back(line);
        }
    }

    LOG_I("Loaded %zu labels from %s", labels.size(), filename.c_str());
    return !labels.empty();
}

// ============================================================================
// 加载配置文件
// ============================================================================
/**
 * 【函数】从 JSON 文件加载配置
 *
 * 配置文件格式:
 * {
 *     "max_seq_len": 64
 * }
 *
 * @return 是否成功
 */
bool IntentRecognizer::load_config() {
    std::string filepath = config_.model_dir + "/" + config_.config_file;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        LOG_W("Could not open config file: %s", filepath.c_str());
        LOG_W("Using default max_seq_len = %d", config_.max_seq_len);
        return true; // 不是致命错误，使用默认值
    }

    try {
        // 使用 nlohmann/json 解析 JSON
        nlohmann::json config_json;
        file >> config_json;

        if (config_json.contains("max_seq_len")) {
            config_.max_seq_len = config_json["max_seq_len"];
            LOG_I("Loaded max_seq_len from config: %d", config_.max_seq_len);
        }
    } catch (const std::exception& e) {
        LOG_W("Failed to parse config JSON: %s", e.what());
        return true; // 使用默认值
    }

    return true;
}

// ============================================================================
// 初始化函数
// ============================================================================
/**
 * 【函数】初始化 Intent 识别器
 *
 * 这是核心初始化函数，做以下事情:
 * 1. 加载配置文件
 * 2. 加载意图标签和槽位标签
 * 3. 加载词汇表
 * 4. 加载 ONNX 模型
 * 5. 获取模型输入输出信息
 *
 * @return 是否成功
 */
bool IntentRecognizer::initialize() {
    if (initialized_) {
        LOG_I("Already initialized");
        return true;
    }

    LOG_I("Initializing IntentRecognizer...");
    LOG_I("Model directory: %s", config_.model_dir.c_str());

    // ============================================================
    // 步骤 1: 加载配置
    // ============================================================
    if (!load_config()) {
        LOG_E("Failed to load config");
        return false;
    }

    // ============================================================
    // 步骤 2: 加载标签
    // ============================================================
    // Intent 标签: play_music, stop_music, set_volume, ...
    if (!load_labels(config_.intent_labels_file, intent_labels_)) {
        LOG_E("Failed to load intent labels");
        return false;
    }

    // Slot 标签: O, B-artist, I-artist, B-song, I-song, ...
    if (!load_labels(config_.slot_labels_file, slot_labels_)) {
        LOG_E("Failed to load slot labels");
        return false;
    }

    // ============================================================
    // 步骤 3: 加载词汇表
    // ============================================================
    // 词汇表格式: 每行一个 token，行号就是 ID
    // [PAD]   (ID=0)
    // [UNK]   (ID=1)
    // [CLS]   (ID=101)
    // ...
    // 你      (ID=1234)
    // 好      (ID=5678)
    std::string vocab_path = config_.model_dir + "/" + config_.vocab_file;
    std::ifstream vocab_file(vocab_path);

    if (!vocab_file.is_open()) {
        LOG_W("Could not open vocab file: %s", vocab_path.c_str());
        LOG_W("Will use simple character-based tokenization");
    } else {
        std::string token;
        int64_t id = 0;

        while (std::getline(vocab_file, token)) {
            // 去除空白
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);

            if (!token.empty()) {
                impl_->vocab[token] = id++;
            }
        }
        LOG_I("Loaded vocabulary with %zu tokens", impl_->vocab.size());

        // 设置特殊 token 的 ID
        if (impl_->vocab.count("[PAD]")) impl_->pad_token_id = impl_->vocab["[PAD]"];
        if (impl_->vocab.count("[CLS]")) impl_->cls_token_id = impl_->vocab["[CLS]"];
        if (impl_->vocab.count("[SEP]")) impl_->sep_token_id = impl_->vocab["[SEP]"];
        if (impl_->vocab.count("[UNK]")) impl_->unk_token_id = impl_->vocab["[UNK]"];
    }

    // ============================================================
    // 步骤 4: 加载 ONNX 模型
    // ============================================================
    std::string model_path = config_.model_dir + "/" + config_.model_file;

    // 尝试加载量化模型，如果不存在则尝试非量化模型
    std::ifstream test_file(model_path);
    if (!test_file.good()) {
        LOG_W("Quantized model not found: %s", model_path.c_str());
        model_path = config_.model_dir + "/joint_model.onnx";
        test_file.open(model_path);
        if (!test_file.good()) {
            LOG_E("Failed to find ONNX model in: %s", config_.model_dir.c_str());
            LOG_E("Tried: joint_model_quantized.onnx and joint_model.onnx");
            return false;
        }
    }
    test_file.close();

    LOG_I("Loading ONNX model from: %s", model_path.c_str());

    // 配置 ONNX Session
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(config_.num_threads);

    // 禁用优化以保证兼容性
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_DISABLE_ALL);
    session_options.SetLogSeverityLevel(2);  // Warning 级别

    LOG_I("Session options configured (threads=%d, optimization=disabled for compatibility)", config_.num_threads);

    // 创建 ONNX Session
    try {
        impl_->session = Ort::Session(impl_->env, model_path.c_str(), session_options);
    } catch (const Ort::Exception& e) {
        LOG_E("Failed to create ONNX session: %s", e.what());
        return false;
    }

    // ============================================================
    // 步骤 5: 获取模型输入输出信息
    // ============================================================
    size_t num_inputs = impl_->session.GetInputCount();
    size_t num_outputs = impl_->session.GetOutputCount();

    LOG_I("Model has %zu inputs and %zu outputs", num_inputs, num_outputs);

    // 获取输入信息
    for (size_t i = 0; i < num_inputs; i++) {
        // 获取输入名称
        auto input_name = impl_->session.GetInputNameAllocated(i, impl_->allocator);
        char* name_str = new char[strlen(input_name.get()) + 1];
        strcpy(name_str, input_name.get());
        impl_->input_names.push_back(name_str);

        // 获取输入形状
        auto type_info = impl_->session.GetInputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        impl_->input_shapes.push_back(tensor_info.GetShape());

        // 打印输入信息
        std::cout << "  Input " << i << ": " << name_str << " shape: [";
        for (size_t j = 0; j < impl_->input_shapes[i].size(); j++) {
            std::cout << impl_->input_shapes[i][j];
            if (j < impl_->input_shapes[i].size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }

    // 获取输出信息
    for (size_t i = 0; i < num_outputs; i++) {
        // 获取输出名称
        auto output_name = impl_->session.GetOutputNameAllocated(i, impl_->allocator);
        char* name_str = new char[strlen(output_name.get()) + 1];
        strcpy(name_str, output_name.get());
        impl_->output_names.push_back(name_str);

        // 获取输出形状
        auto type_info = impl_->session.GetOutputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        impl_->output_shapes.push_back(tensor_info.GetShape());

        // 打印输出信息
        std::cout << "  Output " << i << ": " << name_str << " shape: [";
        for (size_t j = 0; j < impl_->output_shapes[i].size(); j++) {
            std::cout << impl_->output_shapes[i][j];
            if (j < impl_->output_shapes[i].size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }

    initialized_ = true;
    LOG_I("IntentRecognizer initialized successfully!");
    LOG_I("Intent labels: %zu, Slot labels: %zu", intent_labels_.size(), slot_labels_.size());
    return true;
}

// ============================================================================
// 分词函数
// ============================================================================
/**
 * 【函数】将文本分词
 *
 * 使用字符级分词（适合中文）:
 * 输入: "你好"
 * 输出: ["[CLS]", "你", "好", "[SEP]"]
 *
 * @param text - 输入文本
 * @return token 列表
 */
std::vector<std::string> IntentRecognizer::tokenize(const std::string& text) {
    std::vector<std::string> tokens;

    // 添加 [CLS] token (句子开始)
    tokens.push_back("[CLS]");

    // 字符级分词
    std::vector<std::string> chars = utf8_split_chars(text);
    tokens.insert(tokens.end(), chars.begin(), chars.end());

    // 添加 [SEP] token (句子结束)
    tokens.push_back("[SEP]");

    // 如果超过最大长度，截断
    if (tokens.size() > static_cast<size_t>(config_.max_seq_len)) {
        tokens.resize(config_.max_seq_len - 1);
        tokens.push_back("[SEP]");  // 确保以 [SEP] 结尾
    }

    return tokens;
}

// ============================================================================
// Softmax 函数
// ============================================================================
/**
 * 【函数】计算 Softmax
 *
 * Softmax 将原始分数 (logits) 转换为概率分布。
 *
 * 【公式】
 * softmax(x_i) = exp(x_i) / sum(exp(x_j))
 *
 * 【示例】
 * 输入: [2.0, 1.0, 0.1]
 * 输出: [0.659, 0.242, 0.099]  (和为 1.0)
 *
 * @param data - 输入数组 (会被原地修改)
 * @param size - 数组大小
 */
void IntentRecognizer::softmax(float* data, size_t size) {
    // 找最大值 (数值稳定性技巧)
    float max_val = *std::max_element(data, data + size);
    float sum = 0.0f;

    // 计算 exp(x - max) 并累加
    for (size_t i = 0; i < size; i++) {
        data[i] = std::exp(data[i] - max_val);
        sum += data[i];
    }

    // 归一化
    for (size_t i = 0; i < size; i++) {
        data[i] /= sum;
    }
}

// ============================================================================
// 提取槽位
// ============================================================================
/**
 * 【函数】从 BIO 标注中提取槽位
 *
 * 【BIO 标注规则】
 * - B-xxx: 实体开始 (Begin)
 * - I-xxx: 实体内部 (Inside)
 * - O: 非实体 (Outside)
 *
 * 【示例】
 * 输入:
 *   chars: ["播", "放", "周", "杰", "伦", "的", "歌"]
 *   tags:  ["O", "O", "B-artist", "I-artist", "I-artist", "O", "O"]
 * 输出:
 *   [Slot("artist", "周杰伦")]
 *
 * @param chars - 字符列表
 * @param tags  - BIO 标注列表
 * @return 槽位列表
 */
std::vector<Slot> IntentRecognizer::extract_slots(
    const std::vector<std::string>& chars,
    const std::vector<std::string>& tags) {

    std::vector<Slot> slots;
    std::string current_slot_type;  // 当前正在构建的槽位类型
    std::string current_value;       // 当前正在构建的槽位值

    for (size_t i = 0; i < chars.size() && i < tags.size(); i++) {
        const std::string& tag = tags[i];
        const std::string& ch = chars[i];

        if (tag.substr(0, 2) == "B-") {
            // B- 标签: 新实体开始
            // 如果之前有未完成的槽位，先保存
            if (!current_slot_type.empty()) {
                slots.emplace_back(current_slot_type, current_value);
            }

            // 开始新槽位
            current_slot_type = tag.substr(2);  // 去掉 "B-" 前缀
            current_value = ch;

        } else if (tag.substr(0, 2) == "I-" && !current_slot_type.empty()) {
            // I- 标签: 继续当前实体
            if (tag.substr(2) == current_slot_type) {
                // 类型匹配，追加字符
                current_value += ch;
            } else {
                // 类型不匹配 (异常情况)，保存当前槽位并重置
                slots.emplace_back(current_slot_type, current_value);
                current_slot_type.clear();
                current_value.clear();
            }

        } else {
            // O 标签或其他: 实体结束
            if (!current_slot_type.empty()) {
                slots.emplace_back(current_slot_type, current_value);
                current_slot_type.clear();
                current_value.clear();
            }
        }
    }

    // 不要忘记最后一个槽位
    if (!current_slot_type.empty()) {
        slots.emplace_back(current_slot_type, current_value);
    }

    return slots;
}

// ============================================================================
// 预测函数 (核心推理)
// ============================================================================
/**
 * 【函数】执行意图识别预测
 *
 * 这是核心推理函数，完整流程:
 * 1. 分词
 * 2. 转换为 token ID
 * 3. 创建输入张量
 * 4. ONNX 推理
 * 5. 处理输出 (Intent + Slot)
 * 6. 判断是否命中
 *
 * @param text - 输入文本
 * @return 预测结果
 */
PredictionResult IntentRecognizer::predict(const std::string& text) {
    PredictionResult result;
    result.text = text;

    if (!initialized_) {
        std::cerr << "Error: IntentRecognizer not initialized" << std::endl;
        return result;
    }

    // ============================================================
    // 步骤 1: 分词
    // ============================================================
    std::vector<std::string> tokens = tokenize(text);

    // ============================================================
    // 步骤 2: 转换为 token ID
    // ============================================================
    // 创建固定长度的输入数组
    std::vector<int64_t> input_ids(config_.max_seq_len, impl_->pad_token_id);
    std::vector<int64_t> attention_mask(config_.max_seq_len, 0);

    for (size_t i = 0; i < tokens.size(); i++) {
        // 查找 token 的 ID
        if (impl_->vocab.count(tokens[i])) {
            input_ids[i] = impl_->vocab[tokens[i]];
        } else {
            input_ids[i] = impl_->unk_token_id;  // 未知 token
        }
        attention_mask[i] = 1;  // 标记为有效位置
    }

    // ============================================================
    // 步骤 3: 创建输入张量
    // ============================================================
    std::vector<int64_t> input_shape = {1, config_.max_seq_len};  // [batch_size, seq_len]

    // 创建 CPU 内存信息
    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    // 创建输入张量
    std::vector<Ort::Value> input_tensors;

    // 输入 1: input_ids
    input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memory_info, input_ids.data(), input_ids.size(),
        input_shape.data(), input_shape.size()));

    // 输入 2: attention_mask
    input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memory_info, attention_mask.data(), attention_mask.size(),
        input_shape.data(), input_shape.size()));

    // ============================================================
    // 步骤 4: ONNX 推理
    // ============================================================
    try {
        auto output_tensors = impl_->session.Run(
            Ort::RunOptions{nullptr},
            impl_->input_names.data(),
            input_tensors.data(),
            input_tensors.size(),
            impl_->output_names.data(),
            impl_->output_names.size()
        );

        // ============================================================
        // 步骤 5: 处理 Intent 输出
        // ============================================================
        // 输出 0: Intent logits, shape [1, num_intents]
        float* intent_logits = output_tensors[0].GetTensorMutableData<float>();
        size_t intent_size = intent_labels_.size();

        // 应用 Softmax 转换为概率
        softmax(intent_logits, intent_size);

        // 找到概率最高的意图
        size_t intent_idx = std::distance(intent_logits,
            std::max_element(intent_logits, intent_logits + intent_size));

        // 保存原始意图和置信度
        std::string raw_intent = intent_labels_[intent_idx];
        float confidence = intent_logits[intent_idx];

        result.raw_intent = raw_intent;
        result.intent_confidence = confidence;

        // 检查置信度是否达到阈值
        result.hit = (confidence >= config_.confidence_threshold);

        // ============================================================
        // 步骤 6: 处理 Slot 输出
        // ============================================================
        // 输出 1: Slot logits, shape [1, seq_len, num_slot_labels]
        float* slot_logits = output_tensors[1].GetTensorMutableData<float>();
        auto slot_shape = output_tensors[1].GetTensorTypeAndShapeInfo().GetShape();

        size_t seq_len = slot_shape[1];
        size_t num_slot_labels = slot_shape[2];

        // 获取每个位置的预测标签
        std::vector<std::string> slot_tags;
        std::vector<std::string> chars = utf8_split_chars(text);

        // 跳过 [CLS] (i=0) 和 [SEP] (最后一个)
        for (size_t i = 1; i < tokens.size() - 1 && i < seq_len; i++) {
            if (tokens[i] == "[SEP]") break;

            // 获取位置 i 的 logits
            float* logits_at_i = slot_logits + i * num_slot_labels;

            // 找到概率最高的标签
            size_t pred_idx = std::distance(logits_at_i,
                std::max_element(logits_at_i, logits_at_i + num_slot_labels));

            if (pred_idx < slot_labels_.size()) {
                slot_tags.push_back(slot_labels_[pred_idx]);
            }
        }

        result.slot_tags = slot_tags;

        // ============================================================
        // 步骤 7: 根据命中状态设置结果
        // ============================================================
        if (result.hit) {
            result.intent = raw_intent;
            result.slots = extract_slots(chars, slot_tags);
        } else {
            result.intent = "";  // 未命中时意图为空
            result.slots.clear();
        }

    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX inference error: " << e.what() << std::endl;
    }

    return result;
}

// ============================================================================
// 打印结果函数 (调试用)
// ============================================================================
/**
 * 【函数】打印预测结果 (用于调试)
 *
 * @param result     - 预测结果
 * @param show_debug - 是否显示调试信息
 */
void print_result(const PredictionResult& result, bool show_debug) {
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "Input: " << result.text << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    if (result.hit) {
        // 命中: 意图识别成功
        std::cout << "\u2705 HIT! Intent: " << result.intent << std::endl;
        std::cout << "   Confidence: " << (result.intent_confidence * 100.0f) << "%" << std::endl;

        if (!result.slots.empty()) {
            std::cout << "   Slots:" << std::endl;
            for (const auto& slot : result.slots) {
                std::cout << "     - " << slot.slot_type << ": " << slot.slot_value << std::endl;
            }
        }

        std::cout << std::string(60, '-') << std::endl;
        std::cout << "   \u2192 Execute intent handler" << std::endl;
    } else {
        // 未命中: 置信度低于阈值
        std::cout << "\u274c NO HIT (confidence: " << (result.intent_confidence * 100.0f) << "% < threshold)" << std::endl;

        if (show_debug) {
            std::cout << "   [Debug] Best guess was: " << result.raw_intent << std::endl;
        }

        std::cout << std::string(60, '-') << std::endl;
        std::cout << "   \u2192 Fallback to LLaMA inference" << std::endl;
    }

    std::cout << std::string(60, '=') << std::endl << std::endl;
}

} // namespace intent

/**
 * ============================================================================
 * 文件结束
 * ============================================================================
 *
 * 【总结：Intent 识别的核心流程】
 *
 * 1. 加载阶段 (initialize)
 *    - 加载词汇表 (vocab.txt)
 *    - 加载意图标签 (intent_labels.txt)
 *    - 加载槽位标签 (slot_labels.txt)
 *    - 加载 ONNX 模型
 *
 * 2. 推理阶段 (predict)
 *    - 分词: "播放周杰伦的歌" → ["[CLS]", "播", "放", ...]
 *    - 转换 ID: ["[CLS]", ...] → [101, 1234, 5678, ...]
 *    - ONNX 推理: 输入 → 模型 → 输出
 *    - 解析输出:
 *      - Intent: play_music (95%)
 *      - Slots: B-artist, I-artist, ...
 *    - 提取槽位: "周杰伦" → Slot("artist", "周杰伦")
 *
 * 3. 判断阶段
 *    - 置信度 >= 阈值: 命中，返回意图和槽位
 *    - 置信度 < 阈值: 未命中，交给 LLM 处理
 *
 * ============================================================================
 */
