#ifndef INTENT_RECOGNIZER_H
#define INTENT_RECOGNIZER_H

#include <string>
#include <vector>
#include <memory>
#include <map>

namespace intent {

/**
 * Structure to hold a single slot extracted from input text
 */
struct Slot {
    std::string slot_type;   // e.g., "location", "time", "artist"
    std::string slot_value;  // e.g., "北京", "明天", "周杰伦"

    Slot() = default;
    Slot(const std::string& type, const std::string& value)
        : slot_type(type), slot_value(value) {}
};

/**
 * Structure to hold prediction results
 */
struct PredictionResult {
    std::string text;                    // Original input text
    std::string intent;                  // Predicted intent label
    float intent_confidence;             // Confidence score [0, 1]
    std::vector<Slot> slots;             // Extracted slots
    std::vector<std::string> slot_tags;  // BIO tags for each character
};

/**
 * Configuration for the intent recognizer
 */
struct IntentConfig {
    std::string model_dir;               // Directory containing model files
    int max_seq_len = 64;                // Maximum sequence length
    bool use_gpu = false;                // Use GPU acceleration if available
    int num_threads = 4;                 // Number of CPU threads

    // File names (can be overridden)
    std::string model_file = "joint_model_quantized.onnx";
    std::string intent_labels_file = "intent_label.txt";
    std::string slot_labels_file = "slot_label.txt";
    std::string vocab_file = "vocab.txt";
    std::string config_file = "android_config.json";
};

/**
 * Main class for intent recognition and slot filling
 */
class IntentRecognizer {
public:
    /**
     * Constructor
     * @param config Configuration settings
     */
    explicit IntentRecognizer(const IntentConfig& config);

    /**
     * Destructor
     */
    ~IntentRecognizer();

    /**
     * Initialize the recognizer (loads model and labels)
     * @return true if initialization succeeds, false otherwise
     */
    bool initialize();

    /**
     * Predict intent and slots for input text
     * @param text Input text string (UTF-8 encoded)
     * @return Prediction result containing intent and slots
     */
    PredictionResult predict(const std::string& text);

    /**
     * Check if the recognizer is initialized
     * @return true if initialized, false otherwise
     */
    bool is_initialized() const { return initialized_; }

    /**
     * Get list of all supported intent labels
     * @return Vector of intent label strings
     */
    const std::vector<std::string>& get_intent_labels() const { return intent_labels_; }

    /**
     * Get list of all supported slot labels
     * @return Vector of slot label strings
     */
    const std::vector<std::string>& get_slot_labels() const { return slot_labels_; }

private:
    // Internal implementation (PIMPL pattern to hide ONNX Runtime dependencies)
    class Impl;
    std::unique_ptr<Impl> impl_;

    // Configuration
    IntentConfig config_;

    // Labels
    std::vector<std::string> intent_labels_;
    std::vector<std::string> slot_labels_;

    // State
    bool initialized_ = false;

    // Helper methods
    bool load_labels(const std::string& filename, std::vector<std::string>& labels);
    bool load_config();
    std::vector<std::string> tokenize(const std::string& text);
    std::vector<Slot> extract_slots(const std::vector<std::string>& chars,
                                     const std::vector<std::string>& tags);
    void softmax(float* data, size_t size);
};

/**
 * Utility function to print prediction result
 */
void print_result(const PredictionResult& result);

} // namespace intent

#endif // INTENT_RECOGNIZER_H
