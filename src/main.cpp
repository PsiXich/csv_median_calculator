/**
 * \file main.cpp
 * \author github:PsiXich
 * \brief Точка входа приложения для расчета медианы цен из CSV-файлов
 * \date 2025-02-18
 * \version 1.1
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
    
    // Чтение всех CSV файлов
    std::vector<csv_record> all_records;
    
    for (const auto& file_path : csv_files) {
        auto records = csv_reader::read_file(file_path);
        all_records.insert(all_records.end(), 
            records.begin(), records.end());
    }
    
    if (all_records.empty()) {
        spdlog::error("Не удалось прочитать ни одной записи");
        return 1;
    }
    
    spdlog::info("Всего прочитано записей: {}", all_records.size());

    // Сортировка по receive_ts
    spdlog::info("Сортировка данных по временной метке...");
    std::sort(all_records.begin(), all_records.end(),
        [](const csv_record& a_, const csv_record& b_) {
            return get_receive_ts(a_) < get_receive_ts(b_);
        });
    
    spdlog::info("Сортировка завершена");

    // Расчет медианы
    spdlog::info("Начинаю расчет медианы...");
    auto median_results = median_calculator::calculate(all_records);
    
    if (median_results.empty()) {
        spdlog::error("Не удалось рассчитать медиану");
        return 1;
    }
    
    // Сохранение результатов
    const auto output_file = config.output_dir / "median_result.csv";
    if (!median_calculator::save_results(median_results, output_file)) {
        spdlog::error("Не удалось сохранить результаты");
        return 1;
    }
    
    spdlog::info("Завершение работы");
    return 0;
}