#include "intent_recognizer.h"
#include <onnxruntime_cxx_api.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <codecvt>
#include <locale>

// JSON parsing (simple implementation)
#include <nlohmann/json.hpp>

namespace intent {

// UTF-8 utility functions
std::vector<std::string> utf8_split_chars(const std::string& utf8_text) {
    std::vector<std::string> chars;
    size_t i = 0;
    while (i < utf8_text.length()) {
        size_t char_len = 1;
        unsigned char c = utf8_text[i];

        // Determine UTF-8 character length
        if ((c & 0x80) == 0) {          // 0xxxxxxx - ASCII
            char_len = 1;
        } else if ((c & 0xE0) == 0xC0) { // 110xxxxx - 2 bytes
            char_len = 2;
        } else if ((c & 0xF0) == 0xE0) { // 1110xxxx - 3 bytes
            char_len = 3;
        } else if ((c & 0xF8) == 0xF0) { // 11110xxx - 4 bytes
            char_len = 4;
        }

        if (i + char_len <= utf8_text.length()) {
            std::string char_str = utf8_text.substr(i, char_len);
            // Skip whitespace-only characters
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

// PIMPL implementation
class IntentRecognizer::Impl {
public:
    Ort::Env env;
    Ort::Session session{nullptr};
    Ort::AllocatorWithDefaultOptions allocator;

    std::vector<const char*> input_names;
    std::vector<const char*> output_names;
    std::vector<std::vector<int64_t>> input_shapes;
    std::vector<std::vector<int64_t>> output_shapes;

    // Vocabulary map (token -> id)
    std::map<std::string, int64_t> vocab;
    int64_t pad_token_id = 0;
    int64_t cls_token_id = 101;  // [CLS]
    int64_t sep_token_id = 102;  // [SEP]
    int64_t unk_token_id = 100;  // [UNK]

    Impl() : env(ORT_LOGGING_LEVEL_WARNING, "IntentRecognizer") {}

    ~Impl() {
        for (auto name : input_names) delete[] name;
        for (auto name : output_names) delete[] name;
    }
};

IntentRecognizer::IntentRecognizer(const IntentConfig& config)
    : config_(config), impl_(std::make_unique<Impl>()) {
}

IntentRecognizer::~IntentRecognizer() = default;

bool IntentRecognizer::load_labels(const std::string& filename, std::vector<std::string>& labels) {
    std::string filepath = config_.model_dir + "/" + filename;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Failed to open label file: " << filepath << std::endl;
        return false;
    }

    labels.clear();
    std::string line;
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        if (!line.empty()) {
            labels.push_back(line);
        }
    }

    std::cout << "Loaded " << labels.size() << " labels from " << filename << std::endl;
    return !labels.empty();
}

bool IntentRecognizer::load_config() {
    std::string filepath = config_.model_dir + "/" + config_.config_file;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Warning: Could not open config file: " << filepath << std::endl;
        std::cerr << "Using default max_seq_len = " << config_.max_seq_len << std::endl;
        return true; // Not critical, use defaults
    }

    try {
        nlohmann::json config_json;
        file >> config_json;

        if (config_json.contains("max_seq_len")) {
            config_.max_seq_len = config_json["max_seq_len"];
            std::cout << "Loaded max_seq_len from config: " << config_.max_seq_len << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to parse config JSON: " << e.what() << std::endl;
        return true; // Use defaults
    }

    return true;
}

bool IntentRecognizer::initialize() {
    if (initialized_) {
        std::cout << "Already initialized" << std::endl;
        return true;
    }

    std::cout << "Initializing IntentRecognizer..." << std::endl;
    std::cout << "Model directory: " << config_.model_dir << std::endl;

    // Load configuration
    if (!load_config()) {
        return false;
    }

    // Load labels
    if (!load_labels(config_.intent_labels_file, intent_labels_)) {
        return false;
    }
    if (!load_labels(config_.slot_labels_file, slot_labels_)) {
        return false;
    }

    // Load vocabulary
    std::string vocab_path = config_.model_dir + "/" + config_.vocab_file;
    std::ifstream vocab_file(vocab_path);
    if (!vocab_file.is_open()) {
        std::cerr << "Warning: Could not open vocab file: " << vocab_path << std::endl;
        std::cerr << "Will use simple character-based tokenization" << std::endl;
    } else {
        std::string token;
        int64_t id = 0;
        while (std::getline(vocab_file, token)) {
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);
            if (!token.empty()) {
                impl_->vocab[token] = id++;
            }
        }
        std::cout << "Loaded vocabulary with " << impl_->vocab.size() << " tokens" << std::endl;

        // Set special token IDs
        if (impl_->vocab.count("[PAD]")) impl_->pad_token_id = impl_->vocab["[PAD]"];
        if (impl_->vocab.count("[CLS]")) impl_->cls_token_id = impl_->vocab["[CLS]"];
        if (impl_->vocab.count("[SEP]")) impl_->sep_token_id = impl_->vocab["[SEP]"];
        if (impl_->vocab.count("[UNK]")) impl_->unk_token_id = impl_->vocab["[UNK]"];
    }

    // Load ONNX model
    std::string model_path = config_.model_dir + "/" + config_.model_file;

    // Check if quantized model exists, otherwise try non-quantized
    std::ifstream test_file(model_path);
    if (!test_file.good()) {
        model_path = config_.model_dir + "/joint_model.onnx";
        test_file.open(model_path);
        if (!test_file.good()) {
            std::cerr << "Failed to find ONNX model in: " << config_.model_dir << std::endl;
            return false;
        }
    }
    test_file.close();

    std::cout << "Loading ONNX model from: " << model_path << std::endl;

    // Create session options
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(config_.num_threads);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    // Create session
    try {
        impl_->session = Ort::Session(impl_->env, model_path.c_str(), session_options);
    } catch (const Ort::Exception& e) {
        std::cerr << "Failed to create ONNX session: " << e.what() << std::endl;
        return false;
    }

    // Get input/output info
    size_t num_inputs = impl_->session.GetInputCount();
    size_t num_outputs = impl_->session.GetOutputCount();

    std::cout << "Model has " << num_inputs << " inputs and " << num_outputs << " outputs" << std::endl;

    // Get input names and shapes
    for (size_t i = 0; i < num_inputs; i++) {
        auto input_name = impl_->session.GetInputNameAllocated(i, impl_->allocator);
        char* name_str = new char[strlen(input_name.get()) + 1];
        strcpy(name_str, input_name.get());
        impl_->input_names.push_back(name_str);

        auto type_info = impl_->session.GetInputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        impl_->input_shapes.push_back(tensor_info.GetShape());

        std::cout << "  Input " << i << ": " << name_str << " shape: [";
        for (size_t j = 0; j < impl_->input_shapes[i].size(); j++) {
            std::cout << impl_->input_shapes[i][j];
            if (j < impl_->input_shapes[i].size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }

    // Get output names and shapes
    for (size_t i = 0; i < num_outputs; i++) {
        auto output_name = impl_->session.GetOutputNameAllocated(i, impl_->allocator);
        char* name_str = new char[strlen(output_name.get()) + 1];
        strcpy(name_str, output_name.get());
        impl_->output_names.push_back(name_str);

        auto type_info = impl_->session.GetOutputTypeInfo(i);
        auto tensor_info = type_info.GetTensorTypeAndShapeInfo();
        impl_->output_shapes.push_back(tensor_info.GetShape());

        std::cout << "  Output " << i << ": " << name_str << " shape: [";
        for (size_t j = 0; j < impl_->output_shapes[i].size(); j++) {
            std::cout << impl_->output_shapes[i][j];
            if (j < impl_->output_shapes[i].size() - 1) std::cout << ", ";
        }
        std::cout << "]" << std::endl;
    }

    initialized_ = true;
    std::cout << "IntentRecognizer initialized successfully!" << std::endl;
    return true;
}

std::vector<std::string> IntentRecognizer::tokenize(const std::string& text) {
    std::vector<std::string> tokens;

    // Add [CLS] token
    tokens.push_back("[CLS]");

    // Character-level tokenization (for Chinese)
    std::vector<std::string> chars = utf8_split_chars(text);
    tokens.insert(tokens.end(), chars.begin(), chars.end());

    // Add [SEP] token
    tokens.push_back("[SEP]");

    // Truncate if needed
    if (tokens.size() > static_cast<size_t>(config_.max_seq_len)) {
        tokens.resize(config_.max_seq_len - 1);
        tokens.push_back("[SEP]");
    }

    return tokens;
}

void IntentRecognizer::softmax(float* data, size_t size) {
    float max_val = *std::max_element(data, data + size);
    float sum = 0.0f;

    for (size_t i = 0; i < size; i++) {
        data[i] = std::exp(data[i] - max_val);
        sum += data[i];
    }

    for (size_t i = 0; i < size; i++) {
        data[i] /= sum;
    }
}

std::vector<Slot> IntentRecognizer::extract_slots(
    const std::vector<std::string>& chars,
    const std::vector<std::string>& tags) {

    std::vector<Slot> slots;
    std::string current_slot_type;
    std::string current_value;

    for (size_t i = 0; i < chars.size() && i < tags.size(); i++) {
        const std::string& tag = tags[i];
        const std::string& ch = chars[i];

        if (tag.substr(0, 2) == "B-") {
            // Save previous slot if exists
            if (!current_slot_type.empty()) {
                slots.emplace_back(current_slot_type, current_value);
            }
            // Start new slot
            current_slot_type = tag.substr(2);
            current_value = ch;
        } else if (tag.substr(0, 2) == "I-" && !current_slot_type.empty()) {
            // Continue current slot
            if (tag.substr(2) == current_slot_type) {
                current_value += ch;
            } else {
                // Tag mismatch, save and reset
                slots.emplace_back(current_slot_type, current_value);
                current_slot_type.clear();
                current_value.clear();
            }
        } else {
            // O tag or other
            if (!current_slot_type.empty()) {
                slots.emplace_back(current_slot_type, current_value);
                current_slot_type.clear();
                current_value.clear();
            }
        }
    }

    // Don't forget the last slot
    if (!current_slot_type.empty()) {
        slots.emplace_back(current_slot_type, current_value);
    }

    return slots;
}

PredictionResult IntentRecognizer::predict(const std::string& text) {
    PredictionResult result;
    result.text = text;

    if (!initialized_) {
        std::cerr << "Error: IntentRecognizer not initialized" << std::endl;
        return result;
    }

    // Tokenize
    std::vector<std::string> tokens = tokenize(text);

    // Convert tokens to IDs
    std::vector<int64_t> input_ids(config_.max_seq_len, impl_->pad_token_id);
    std::vector<int64_t> attention_mask(config_.max_seq_len, 0);

    for (size_t i = 0; i < tokens.size(); i++) {
        if (impl_->vocab.count(tokens[i])) {
            input_ids[i] = impl_->vocab[tokens[i]];
        } else {
            input_ids[i] = impl_->unk_token_id;
        }
        attention_mask[i] = 1;
    }

    // Create input tensors
    std::vector<int64_t> input_shape = {1, config_.max_seq_len};

    auto memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    std::vector<Ort::Value> input_tensors;
    input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memory_info, input_ids.data(), input_ids.size(),
        input_shape.data(), input_shape.size()));
    input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memory_info, attention_mask.data(), attention_mask.size(),
        input_shape.data(), input_shape.size()));

    // Run inference
    try {
        auto output_tensors = impl_->session.Run(
            Ort::RunOptions{nullptr},
            impl_->input_names.data(),
            input_tensors.data(),
            input_tensors.size(),
            impl_->output_names.data(),
            impl_->output_names.size()
        );

        // Process intent output
        float* intent_logits = output_tensors[0].GetTensorMutableData<float>();
        size_t intent_size = intent_labels_.size();

        softmax(intent_logits, intent_size);

        size_t intent_idx = std::distance(intent_logits,
            std::max_element(intent_logits, intent_logits + intent_size));

        result.intent = intent_labels_[intent_idx];
        result.intent_confidence = intent_logits[intent_idx];

        // Process slot output
        float* slot_logits = output_tensors[1].GetTensorMutableData<float>();
        auto slot_shape = output_tensors[1].GetTensorTypeAndShapeInfo().GetShape();

        size_t seq_len = slot_shape[1];
        size_t num_slot_labels = slot_shape[2];

        // Get predicted slot tags
        std::vector<std::string> slot_tags;
        std::vector<std::string> chars = utf8_split_chars(text);

        for (size_t i = 1; i < tokens.size() - 1 && i < seq_len; i++) { // Skip [CLS] and [SEP]
            if (tokens[i] == "[SEP]") break;

            float* logits_at_i = slot_logits + i * num_slot_labels;
            size_t pred_idx = std::distance(logits_at_i,
                std::max_element(logits_at_i, logits_at_i + num_slot_labels));

            if (pred_idx < slot_labels_.size()) {
                slot_tags.push_back(slot_labels_[pred_idx]);
            }
        }

        result.slot_tags = slot_tags;
        result.slots = extract_slots(chars, slot_tags);

    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX inference error: " << e.what() << std::endl;
    }

    return result;
}

void print_result(const PredictionResult& result) {
    std::cout << std::string(50, '=') << std::endl;
    std::cout << "Input: " << result.text << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    std::cout << "Intent: " << result.intent
              << " (confidence: " << (result.intent_confidence * 100.0f) << "%)" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    if (!result.slots.empty()) {
        std::cout << "Slots:" << std::endl;
        for (const auto& slot : result.slots) {
            std::cout << "  - " << slot.slot_type << ": " << slot.slot_value << std::endl;
        }
    } else {
        std::cout << "Slots: (none)" << std::endl;
    }

    std::cout << std::string(50, '-') << std::endl;

    if (!result.slot_tags.empty()) {
        std::cout << "BIO Tags:" << std::endl;
        std::vector<std::string> chars = utf8_split_chars(result.text);

        std::cout << "  Chars: ";
        for (size_t i = 0; i < chars.size() && i < result.slot_tags.size(); i++) {
            std::cout << chars[i] << " ";
        }
        std::cout << std::endl;

        std::cout << "  Tags:  ";
        for (const auto& tag : result.slot_tags) {
            std::cout << tag << " ";
        }
        std::cout << std::endl;
    }

    std::cout << std::string(50, '=') << std::endl << std::endl;
}

} // namespace intent
