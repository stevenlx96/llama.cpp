#include "intent_recognizer.h"
#include <iostream>
#include <string>
#include <vector>

using namespace intent;

void run_test_cases(IntentRecognizer& recognizer) {
    std::vector<std::string> test_cases = {
        // Weather
        "今天北京天气怎么样",
        "明天上海会下雨吗",
        "深圳这周末的天气如何",
        "广州现在的温度是多少",

        // Music (if your model supports it)
        "播放周杰伦的歌",
        "我想听一首轻音乐",
        "放一首Beyond的海阔天空",

        // General
        "你好",
        "今天星期几",
    };

    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "Running test cases..." << std::endl;
    std::cout << std::string(50, '=') << "\n" << std::endl;

    for (const auto& text : test_cases) {
        try {
            auto result = recognizer.predict(text);
            print_result(result);
        } catch (const std::exception& e) {
            std::cerr << "Error processing '" << text << "': " << e.what() << std::endl;
        }
    }
}

void interactive_mode(IntentRecognizer& recognizer) {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "Interactive Mode" << std::endl;
    std::cout << "Enter text to predict, or 'quit' to exit" << std::endl;
    std::cout << std::string(50, '=') << "\n" << std::endl;

    std::string input;
    while (true) {
        std::cout << "Input: ";
        std::getline(std::cin, input);

        // Trim whitespace
        input.erase(0, input.find_first_not_of(" \t\r\n"));
        input.erase(input.find_last_not_of(" \t\r\n") + 1);

        if (input == "quit" || input == "exit" || input == "q") {
            std::cout << "Bye!" << std::endl;
            break;
        }

        if (input.empty()) {
            continue;
        }

        try {
            auto result = recognizer.predict(input);
            print_result(result);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  --model_dir PATH    Directory containing ONNX model and assets" << std::endl;
    std::cout << "                      (default: ./data/file/models/intend)" << std::endl;
    std::cout << "  --interactive, -i   Run in interactive mode" << std::endl;
    std::cout << "  --text TEXT         Single text to predict" << std::endl;
    std::cout << "  --threads N         Number of CPU threads (default: 4)" << std::endl;
    std::cout << "  --help, -h          Show this help message" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  " << program_name << " --model_dir ./models/intent" << std::endl;
    std::cout << "  " << program_name << " --text \"今天北京天气怎么样\"" << std::endl;
    std::cout << "  " << program_name << " --interactive" << std::endl;
}

int main(int argc, char** argv) {
    // Parse command line arguments
    IntentConfig config;
    config.model_dir = "./data/file/models/intend";
    bool interactive = false;
    std::string single_text;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--model_dir" && i + 1 < argc) {
            config.model_dir = argv[++i];
        } else if (arg == "--interactive" || arg == "-i") {
            interactive = true;
        } else if (arg == "--text" && i + 1 < argc) {
            single_text = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            config.num_threads = std::atoi(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Initialize recognizer
    std::cout << "Initializing Intent Recognizer..." << std::endl;
    IntentRecognizer recognizer(config);

    if (!recognizer.initialize()) {
        std::cerr << "Failed to initialize recognizer" << std::endl;
        return 1;
    }

    // Run appropriate mode
    if (!single_text.empty()) {
        // Single prediction
        auto result = recognizer.predict(single_text);
        print_result(result);
    } else if (interactive) {
        // Interactive mode
        interactive_mode(recognizer);
    } else {
        // Run test cases
        run_test_cases(recognizer);
    }

    return 0;
}
