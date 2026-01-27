#include "intent_recognizer.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

using namespace intent;

void run_test_cases(IntentRecognizer& recognizer, bool show_debug = true) {
    std::vector<std::string> test_cases = {
        // Should HIT - Weather
        "今天北京天气怎么样",
        "明天上海会下雨吗",
        "深圳这周末的天气如何",

        // Should HIT - Music (if your model supports it)
        "播放周杰伦的歌",
        "我想听一首轻音乐",

        // Should NOT HIT - Random/Chat
        "你好",
        "去哪吃饭",
        "卧槽",
        "今天股票涨了吗",
        "帮我写一首诗",
        "1+1等于几",
    };

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Running test cases (threshold: "
              << (recognizer.get_threshold() * 100.0f) << "%)" << std::endl;
    std::cout << std::string(60, '=') << "\n" << std::endl;

    int hit_count = 0;
    for (const auto& text : test_cases) {
        try {
            auto result = recognizer.predict(text);
            print_result(result, show_debug);
            if (result.hit) {
                hit_count++;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing '" << text << "': " << e.what() << std::endl;
        }
    }

    std::cout << "\nSummary: " << hit_count << "/" << test_cases.size() << " hit" << std::endl;
}

void interactive_mode(IntentRecognizer& recognizer) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Interactive Mode" << std::endl;
    std::cout << "Current threshold: " << (recognizer.get_threshold() * 100.0f) << "%" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  'quit' or 'q'     - Exit" << std::endl;
    std::cout << "  'threshold 0.8'   - Change threshold to 80%" << std::endl;
    std::cout << "  'debug'           - Toggle debug mode" << std::endl;
    std::cout << "  Or enter any text to predict" << std::endl;
    std::cout << std::string(60, '=') << "\n" << std::endl;

    bool show_debug = false;
    std::string input;

    while (true) {
        std::cout << ">>> ";
        std::getline(std::cin, input);

        // Trim whitespace
        input.erase(0, input.find_first_not_of(" \t\r\n"));
        input.erase(input.find_last_not_of(" \t\r\n") + 1);

        if (input.empty()) {
            continue;
        }

        if (input == "quit" || input == "exit" || input == "q") {
            std::cout << "Bye!" << std::endl;
            break;
        }

        if (input == "debug") {
            show_debug = !show_debug;
            std::cout << "Debug mode: " << (show_debug ? "ON" : "OFF") << "\n" << std::endl;
            continue;
        }

        if (input.substr(0, 10) == "threshold ") {
            try {
                float new_threshold = std::stof(input.substr(10));
                recognizer.set_threshold(new_threshold);
                std::cout << "Threshold updated to: " << (new_threshold * 100.0f) << "%\n" << std::endl;
            } catch (...) {
                std::cerr << "Usage: threshold 0.8\n" << std::endl;
            }
            continue;
        }

        try {
            auto result = recognizer.predict(input);
            print_result(result, show_debug);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n" << std::endl;
        }
    }
}

void demo_integration(IntentRecognizer& recognizer) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "Integration Demo: Intent Router + LLaMA Fallback" << std::endl;
    std::cout << std::string(60, '=') << "\n" << std::endl;

    // Mock handler functions
    auto mock_llama = [](const std::string& text) -> std::string {
        return "[LLaMA Response] 这是对「" + text + "」的通用回复...";
    };

    auto handle_weather = [](const std::vector<Slot>& slots) -> std::string {
        std::string city = "当前位置";
        std::string date = "今天";

        for (const auto& slot : slots) {
            if (slot.slot_type == "location" || slot.slot_type == "city") {
                city = slot.slot_value;
            } else if (slot.slot_type == "time" || slot.slot_type == "date") {
                date = slot.slot_value;
            }
        }

        return "[Weather API] " + date + city + "的天气是：晴，25°C";
    };

    auto handle_music = [](const std::vector<Slot>& slots) -> std::string {
        for (const auto& slot : slots) {
            if (slot.slot_type == "singer" || slot.slot_type == "artist") {
                return "[Music API] 正在播放" + slot.slot_value + "的歌曲...";
            } else if (slot.slot_type == "song") {
                return "[Music API] 正在播放《" + slot.slot_value + "》...";
            }
        }
        return "[Music API] 正在播放随机音乐...";
    };

    // Process function
    auto process_input = [&](const std::string& text) -> std::string {
        auto result = recognizer.predict(text);

        if (!result.hit) {
            // Fallback to LLaMA
            return mock_llama(text);
        }

        // Route to specific handler based on intent
        if (result.intent.find("weather") != std::string::npos) {
            return handle_weather(result.slots);
        } else if (result.intent.find("music") != std::string::npos) {
            return handle_music(result.slots);
        } else {
            // Unknown intent domain, fallback
            return mock_llama(text);
        }
    };

    // Test cases
    std::vector<std::string> test_inputs = {
        "今天北京天气怎么样",
        "播放周杰伦的歌",
        "去哪吃饭",
        "帮我写一首诗",
    };

    for (const auto& text : test_inputs) {
        std::cout << "User: " << text << std::endl;
        std::string response = process_input(text);
        std::cout << "System: " << response << std::endl;
        std::cout << std::endl;
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  --model_dir PATH    Directory containing ONNX model and assets" << std::endl;
    std::cout << "                      (default: ./data/file/models/intend)" << std::endl;
    std::cout << "  --threshold N       Confidence threshold for intent hit (0.0-1.0)" << std::endl;
    std::cout << "                      (default: 0.6)" << std::endl;
    std::cout << "  --interactive, -i   Run in interactive mode" << std::endl;
    std::cout << "  --demo              Run integration demo" << std::endl;
    std::cout << "  --text TEXT         Single text to predict" << std::endl;
    std::cout << "  --threads N         Number of CPU threads (default: 4)" << std::endl;
    std::cout << "  --debug             Show debug info (raw intent when !hit)" << std::endl;
    std::cout << "  --help, -h          Show this help message" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  " << program_name << " --model_dir ./models/intent" << std::endl;
    std::cout << "  " << program_name << " --text \"今天北京天气怎么样\"" << std::endl;
    std::cout << "  " << program_name << " --interactive --threshold 0.8" << std::endl;
    std::cout << "  " << program_name << " --demo" << std::endl;
}

int main(int argc, char** argv) {
    // Parse command line arguments
    IntentConfig config;
    config.model_dir = "./data/file/models/intend";
    bool interactive = false;
    bool demo = false;
    bool show_debug = false;
    std::string single_text;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--model_dir" && i + 1 < argc) {
            config.model_dir = argv[++i];
        } else if (arg == "--threshold" && i + 1 < argc) {
            config.confidence_threshold = std::stof(argv[++i]);
        } else if (arg == "--interactive" || arg == "-i") {
            interactive = true;
        } else if (arg == "--demo") {
            demo = true;
        } else if (arg == "--text" && i + 1 < argc) {
            single_text = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            config.num_threads = std::atoi(argv[++i]);
        } else if (arg == "--debug") {
            show_debug = true;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Initialize recognizer
    std::cout << "Initializing Intent Recognizer..." << std::endl;
    std::cout << "Confidence threshold: " << (config.confidence_threshold * 100.0f) << "%" << std::endl;

    IntentRecognizer recognizer(config);

    if (!recognizer.initialize()) {
        std::cerr << "Failed to initialize recognizer" << std::endl;
        return 1;
    }

    // Run appropriate mode
    if (!single_text.empty()) {
        // Single prediction
        auto result = recognizer.predict(single_text);
        print_result(result, show_debug);
    } else if (demo) {
        // Integration demo
        demo_integration(recognizer);
    } else if (interactive) {
        // Interactive mode
        interactive_mode(recognizer);
    } else {
        // Run test cases
        run_test_cases(recognizer, show_debug);
    }

    return 0;
}
