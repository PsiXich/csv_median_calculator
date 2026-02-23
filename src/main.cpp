/**
 * \file main.cpp
 * \author github:PsiXich
 * \brief Точка входа приложения для расчета медианы цен из CSV-файлов
 * \date 2025-02-23
 * \version 2.1
 */

#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>
#include <chrono>
#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>
#include "config_parser.hpp"
#include "file_finder.hpp"
#include "csv_record.hpp"
#include "csv_reader.hpp"
#include "median_calculator.hpp"
#include "thread_safe_queue.hpp"
#include "parallel_csv_reader.hpp"
#include "data_processor.hpp"
#include "streaming_csv_reader.hpp"
#include "streaming_parallel_reader.hpp"

namespace po = boost::program_options;
namespace fs = std::filesystem;

/**
 * \brief Парсинг аргументов командной строки
 * \param argc_ количество аргументов
 * \param argv_ массив аргументов
 * \return путь к конфигурационному файлу (пустой при ошибке)
 */
[[nodiscard]] fs::path parse_command_line(int argc_, char* argv_[]) noexcept {
    try {
        po::options_description desc("Available options");
        desc.add_options()
            ("help,h", "Show help message")
            ("config,c", po::value<std::string>(), 
                "Path to configuration file")
            ("cfg", po::value<std::string>(), 
                "Path to configuration file (short form)")
            ("single-thread,s", "Use single-threaded mode (v1.0)")
            ("streaming,m", "Use memory-efficient streaming mode (v2.1)")
            ("batch-size", po::value<int>()->default_value(10000),
                "Batch size for streaming mode (default: 10000)")
            ("benchmark,b", "Run benchmark (compare all modes)");
        
        po::variables_map vm;
        po::store(po::parse_command_line(argc_, argv_, desc), vm);
        po::notify(vm);
        
        // Обработка справки
        if (vm.count("help")) {
            std::cout << desc << "\n";
            std::cout << "\nExamples:\n";
            std::cout << "  Multi-threaded:  ./csv_median_calculator -config config.toml\n";
            std::cout << "  Single-threaded: ./csv_median_calculator -s\n";
            std::cout << "  Streaming mode:  ./csv_median_calculator -m\n";
            std::cout << "  Benchmark:       ./csv_median_calculator -b\n";
            std::cout << "\nStreaming mode (v2.1):\n";
            std::cout << "  Best for files >RAM (e.g., 10GB file with 8GB RAM)\n";
            std::cout << "  Processes data in batches without loading entire file\n";
            return "";
        }
        
        // Получение пути к конфигурации
        fs::path config_path;
        
        if (vm.count("config")) {
            config_path = vm["config"].as<std::string>();
        } else if (vm.count("cfg")) {
            config_path = vm["cfg"].as<std::string>();
        } else {
            // По умолчанию ищем config.toml в текущей директории
            config_path = fs::current_path() / "config.toml";
            spdlog::info("Config file not specified, using: {}", 
                config_path.string());
        }
        
        return config_path;
        
    } catch (const std::exception& err) {
        spdlog::error("Error parsing command line arguments: {}", 
            err.what());
        return "";
    }
}

/**
 * \brief Проверка существования директории и создание при необходимости
 * \param path_ путь к директории
 * \param description_ описание для логов
 * \return true если директория существует или успешно создана
 */
[[nodiscard]] bool ensure_directory_exists(
    const fs::path& path_,
    const std::string& description_) noexcept {
    
    try {
        if (fs::exists(path_)) {
            if (!fs::is_directory(path_)) {
                spdlog::error("{} exists but is not a directory: {}", 
                    description_, path_.string());
                return false;
            }
            return true;
        }
        
        // Директория не существует, создаём её
        fs::create_directories(path_);
        spdlog::info("Created {} directory: {}", 
            description_, path_.string());
        return true;
        
    } catch (const fs::filesystem_error& err) {
        spdlog::error("Failed to create {} directory: {}", 
            description_, err.what());
        return false;
    }
}

/**
 * \brief Однопоточная обработка (v1.0 алгоритм)
 * \param csv_files список файлов
 * \return вектор результатов
 */
[[nodiscard]] std::vector<median_result> process_single_threaded(
    const std::vector<fs::path>& csv_files) {
    
    spdlog::info("Using SINGLE-THREADED mode (v1.0)");
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Чтение всех CSV файлов последовательно
    std::vector<csv_record> all_records;
    
    for (const auto& file_path : csv_files) {
        auto records = csv_reader::read_file(file_path);
        all_records.insert(all_records.end(), 
            records.begin(), records.end());
    }
    
    if (all_records.empty()) {
        spdlog::error("Failed to read any records");
        return {};
    }
    
    spdlog::info("Total records read: {}", all_records.size());

    // Сортировка по receive_ts
    spdlog::info("Sorting data by timestamp...");
    std::sort(all_records.begin(), all_records.end(),
        [](const csv_record& a_, const csv_record& b_) {
            return get_receive_ts(a_) < get_receive_ts(b_);
        });
    
    spdlog::info("Sorting completed");

    // Расчет медианы
    spdlog::info("Starting median calculation...");
    auto median_results = median_calculator::calculate(all_records);
    
    auto end_time = std::chrono::steady_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    spdlog::info("Single-threaded processing completed in {} ms", 
        total_time.count());
    
    return median_results;
}

/**
 * \brief Многопоточная обработка (v2.0 алгоритм)
 * \param csv_files список файлов
 * \return вектор результатов
 */
[[nodiscard]] std::vector<median_result> process_multi_threaded(
    const std::vector<fs::path>& csv_files) {
    
    spdlog::info("Using MULTI-THREADED mode (v2.0)");
    spdlog::info("Hardware threads available: {}", 
        std::thread::hardware_concurrency());
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Создаём потокобезопасную очередь
    auto queue = std::make_shared<thread_safe_queue<csv_record>>();
    
    // Создаём параллельный читатель
    parallel_csv_reader reader(queue);
    
    // Запускаем чтение в отдельных потоках
    auto read_stats = reader.read_files(csv_files);
    
    // Обрабатываем данные из очереди
    data_processor processor(queue);
    auto median_results = processor.process_and_calculate();
    
    auto end_time = std::chrono::steady_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    spdlog::info("Multi-threaded processing completed in {} ms", 
        total_time.count());
    
    return median_results;
}

/**
 * \brief Запуск benchmark сравнения однопоточной и многопоточной версий
 * \param csv_files список файлов
 * \param output_dir директория для результатов
 */
void run_benchmark(
    const std::vector<fs::path>& csv_files,
    const fs::path& output_dir) {
    
    spdlog::info("========================================");
    spdlog::info("  BENCHMARK: Single-thread vs Multi-thread");
    spdlog::info("========================================");
    spdlog::info("Files: {}", csv_files.size());
    spdlog::info("");
    
    // Однопоточная версия
    spdlog::info("--- Running Single-threaded version ---");
    auto st_start = std::chrono::steady_clock::now();
    auto st_results = process_single_threaded(csv_files);
    auto st_end = std::chrono::steady_clock::now();
    auto st_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        st_end - st_start);
    
    spdlog::info("");
    
    // Многопоточная версия
    spdlog::info("--- Running Multi-threaded version ---");
    auto mt_start = std::chrono::steady_clock::now();
    auto mt_results = process_multi_threaded(csv_files);
    auto mt_end = std::chrono::steady_clock::now();
    auto mt_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        mt_end - mt_start);
    
    spdlog::info("");
    spdlog::info("========================================");
    spdlog::info("  BENCHMARK RESULTS");
    spdlog::info("========================================");
    spdlog::info("Single-threaded: {} ms", st_time.count());
    spdlog::info("Multi-threaded:  {} ms", mt_time.count());
    
    if (mt_time.count() > 0) {
        double speedup = static_cast<double>(st_time.count()) / mt_time.count();
        spdlog::info("Speedup:         {:.2f}x", speedup);
        
        if (speedup > 1.0) {
            spdlog::info("Multi-threaded is {:.1f}% FASTER", (speedup - 1.0) * 100);
        } else {
            spdlog::info("Single-threaded is {:.1f}% faster", (1.0 / speedup - 1.0) * 100);
        }
    }
    spdlog::info("========================================");
    
    // Сохраняем результаты многопоточной версии
    const auto output_file = output_dir / "median_result.csv";
    if (!median_calculator::save_results(mt_results, output_file)) {
    spdlog::error("Failed to save benchmark results");
}
}

int main(int argc, char* argv[]) {
    // Настройка логирования
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    
    spdlog::info("Starting csv_median_calculator v1.0.0");
    
    // Проверка аргументов для выбора режима
    bool single_thread_mode = false;
    bool benchmark_mode = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-s" || arg == "--single-thread") {
            single_thread_mode = true;
        }
        if (arg == "-b" || arg == "--benchmark") {
            benchmark_mode = true;
        }
    }

    // Парсинг аргументов командной строки
    auto config_path = parse_command_line(argc, argv);
    if (config_path.empty()) {
        return 1;
    }
    
    spdlog::info("Reading configuration: {}", config_path.string());
    
    // Парсинг конфигурационного файла
    auto config_opt = config_parser::parse(config_path);
    if (!config_opt) {
        spdlog::error("Failed to load configuration");
        return 1;
    }
    
    const auto& config = *config_opt;
    
    // Вывод конфигурации
    spdlog::info("Input directory: {}", config.input_dir.string());
    spdlog::info("Output directory: {}", config.output_dir.string());
    
    // В spdlog есть встроенная поддержка fmt
    if (!config.filename_mask.empty()) {
        spdlog::info("File masks: {}", 
            fmt::join(config.filename_mask, ", "));
    } else {
        spdlog::info("No file masks specified, all CSV files will be processed");
    }
    
    // Проверка и создание входной директории при необходимости
    if (!ensure_directory_exists(config.input_dir, "input")) {
        spdlog::error("Cannot proceed without input directory");
        return 1;
    }
    
    // Проверка и создание выходной директории при необходимости
    if (!ensure_directory_exists(config.output_dir, "output")) {
        spdlog::error("Cannot create output directory");
        return 1;
    }
    
    // Поиск CSV файлов
    auto csv_files = file_finder::find_csv_files(
        config.input_dir, 
        config.filename_mask);
    
    if (csv_files.empty()) {
        spdlog::warn("No CSV files found in directory: {}", 
            config.input_dir.string());
        spdlog::info("Please add CSV files to: {}", 
            config.input_dir.string());
        spdlog::info("You can generate test data using: generate_test_data.exe");
        return 0;
    }
    
    // Выбор режима обработки
    std::vector<median_result> median_results;
    
    if (benchmark_mode) {
        // Режим бенчмарка
        run_benchmark(csv_files, config.output_dir);
        
    } else if (single_thread_mode) {
        // Однопоточный режим
        median_results = process_single_threaded(csv_files);
        
        if (median_results.empty()) {
            spdlog::error("Failed to calculate median");
            return 1;
        }
        
        // Сохранение результатов
        const auto output_file = config.output_dir / "median_result.csv";
        if (!median_calculator::save_results(median_results, output_file)) {
            spdlog::error("Failed to save results");
            return 1;
        }
        
    } else {
        // Многопоточный режим (по умолчанию)
        median_results = process_multi_threaded(csv_files);
        
        if (median_results.empty()) {
            spdlog::error("Failed to calculate median");
            return 1;
        }
        
        // Сохранение результатов
        const auto output_file = config.output_dir / "median_result.csv";
        if (!median_calculator::save_results(median_results, output_file)) {
            spdlog::error("Failed to save results");
            return 1;
        }
    }
    
    spdlog::info("Application finished successfully");
    return 0;
}