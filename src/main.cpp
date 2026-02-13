/**
 * \file main.cpp
 * \author github:PsiXich
 * \brief Точка входа приложения для расчета медианы цен из CSV-файлов
 * \date 2025-02-12
 * \version 1.0
 */

#include <iostream>
#include <filesystem>
#include <string>
#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>
#include "config_parser.hpp"
#include "file_finder.hpp"

namespace po = boost::program_options;
namespace fs = std::filesystem;

/**
 * \brief Парсинг аргументов командной строки
 * \param argc_ количество аргументов
 * \param argv_ массив аргументов
 * \return путь к конфигурационному файлу(пустой при ошибке)
 */
[[nodiscard]] fs::path parse_command_line(int argc_, char* argv_[]) noexcept {
    try {
        po::options_description desc("Допустимые опции");
        desc.add_options()
            ("help,h", "Показать справку")
            ("config,c", po::value<std::string>(), 
                "Путь к конфигурационному файлу")
            ("cfg", po::value<std::string>(), 
                "Путь к конфигурационному файлу (короткая форма)");
        
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
            // По умолчанию ищем config.toml в директории с исполняемым файлом
            config_path = fs::current_path() / "config.toml";
            spdlog::info("Конфигурационный файл не указан, используется: {}", 
                config_path.string());
        }
        
        return config_path;
        
    } catch (const std::exception& err) {
        spdlog::error("Ошибка парсинга аргументов командной строки: {}", 
            err.what());
        return "";
    }
}

int main(int argc, char* argv[]) {
    // Настройка логирования
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    
    spdlog::info("Запуск приложения csv_median_calculator v1.0.0");
    
    // Парсинг аргументов командной строки
    auto config_path = parse_command_line(argc, argv);
    if (config_path.empty()) {
        return 1;
    }
    
    spdlog::info("Чтение конфигурации: {}", config_path.string());
    
    // Парсинг конфигурационного файла
    auto config_opt = config_parser::parse(config_path);
    if (!config_opt) {
        spdlog::error("Не удалось загрузить конфигурацию");
        return 1;
    }
    
    const auto& config = *config_opt;
    
    // Вывод конфигурации
    spdlog::info("Входная директория: {}", config.input_dir.string());
    spdlog::info("Выходная директория: {}", config.output_dir.string());
    
    //В spdlog есть поддержка fmt
    if (!config.filename_mask.empty()) {
        spdlog::info("Маски файлов: {}", 
            fmt::join(config.filename_mask, ", "));
    } else {
        spdlog::info("Маски файлов не указаны, будут обработаны все CSV файлы");
    }
    
    // Валидация директорий
    if (!fs::exists(config.input_dir)) {
        spdlog::error("Входная директория не существует: {}", 
            config.input_dir.string());
        return 1;
    }
    
    if (!fs::is_directory(config.input_dir)) {
        spdlog::error("Путь не является директорией: {}", 
            config.input_dir.string());
        return 1;
    }
    
    // Создание выходной директории если не существует
    if (!fs::exists(config.output_dir)) {
        try {
            fs::create_directories(config.output_dir);
            spdlog::info("Создана выходная директория: {}", 
                config.output_dir.string());
        } catch (const fs::filesystem_error& err) {
            spdlog::error("Не удалось создать выходную директорию: {}", 
                err.what());
            return 1;
        }
    }
    
    // Поиск CSV файлов
    auto csv_files = file_finder::find_csv_files(
        config.input_dir, 
        config.filename_mask);
    
    if (csv_files.empty()) {
        spdlog::warn("CSV файлы не найдены в директории: {}", 
            config.input_dir.string());
        return 0;
    }
    
    // TODO: Реализовать обработку файлов
    
    spdlog::info("Завершение работы");
    return 0;
}