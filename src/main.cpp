/**
 * \file main.cpp
 * \author github:PsiXich
 * \brief Точка входа приложения для расчета медианы цен из CSV-файлов
 * \date 2025-02-21
 * \version 1.3
 */

#include <iostream>
#include <filesystem>
#include <string>
#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>
#include "config_parser.hpp"
#include "file_finder.hpp"
#include "csv_record.hpp"
#include "csv_reader.hpp"
#include "median_calculator.hpp"

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
                "Path to configuration file (short form)");
        
        po::variables_map vm;
        po::store(po::parse_command_line(argc_, argv_, desc), vm);
        po::notify(vm);
        
        // Обработка справки
        if (vm.count("help")) {
            std::cout << desc << "\n";
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

int main(int argc, char* argv[]) {
    // Настройка логирования
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    
    spdlog::info("Starting csv_median_calculator v1.0.0");
    
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
    
    // Чтение всех CSV файлов
    std::vector<csv_record> all_records;
    
    for (const auto& file_path : csv_files) {
        auto records = csv_reader::read_file(file_path);
        all_records.insert(all_records.end(), 
            records.begin(), records.end());
    }
    
    if (all_records.empty()) {
        spdlog::error("Failed to read any records");
        return 1;
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
    
    spdlog::info("Application finished successfully");
    return 0;
}