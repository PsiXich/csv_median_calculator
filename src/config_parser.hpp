/**
 * \file config_parser.hpp
 * \author github:PsiXich
 * \brief Парсер конфигурационного файла TOML
 * \date 2025-02-12
 * \version 1.0
 */

#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include <toml++/toml.hpp>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

/**
 * \brief Структура конфигурации приложения
 */
struct app_config {
    fs::path input_dir;                      ///< Директория с входными CSV
    fs::path output_dir;                     ///< Директория для выходных файлов
    std::vector<std::string> filename_mask;  ///< Маски имен файлов
    
    /**
     * \brief Валидация конфигурации
     * \return true если конфигурация корректна
     */
    [[nodiscard]] bool is_valid() const noexcept {
        return !input_dir.empty();
    }
};

/**
 * \brief Парсер конфигурационного файла TOML
 */
class config_parser {
public:
    /**
     * \brief Парсинг конфигурационного файла
     * \param config_path_ путь к файлу конфигурации
     * \return опциональная конфигурация (nullopt при ошибке)
     */
    [[nodiscard]] static std::optional<app_config> parse(
        const fs::path& config_path_) noexcept;

private:
    /**
     * \brief Извлечение пути из TOML таблицы
     * \param table_ TOML таблица
     * \param key_ ключ
     * \return опциональный путь
     */
    [[nodiscard]] static std::optional<fs::path> extract_path(
        const toml::table& table_,
        const std::string& key_) noexcept;
    
    /**
     * \brief Извлечение массива строк из TOML таблицы
     * \param table_ TOML таблица
     * \param key_ ключ
     * \return вектор строк (пустой при ошибке)
     */
    [[nodiscard]] static std::vector<std::string> extract_string_array(
        const toml::table& table_,
        const std::string& key_) noexcept;
};

// ============================================================================
// Реализация
// ============================================================================

std::optional<app_config> config_parser::parse(
    const fs::path& config_path_) noexcept {
    
    try {
        // Проверка существования файла
        if (!fs::exists(config_path_)) {
            spdlog::error("Configuration file not found: {}", 
                config_path_.string());
            return std::nullopt;
        }
        
        // Парсинг TOML файла
        auto config = toml::parse_file(config_path_.string());
        
        // Получение секции [main]
        auto main_section = config["main"];
        if (!main_section) {
            spdlog::error("Section [main] not found in configuration");
            return std::nullopt;
        }
        
        const auto& main_table = *main_section.as_table();
        
        // Извлечение параметров
        app_config result;
        
        // input (обязательный параметр)
        auto input_opt = extract_path(main_table, "input");
        if (!input_opt) {
            spdlog::error("The ‘input’ parameter is mandatory in the configuration");
            return std::nullopt;
        }
        result.input_dir = *input_opt;
        
        // output (опциональный параметр)
        auto output_opt = extract_path(main_table, "output");
        if (output_opt) {
            result.output_dir = *output_opt;
        } else {
            // По умолчанию создаем директорию output в текущей директории
            result.output_dir = fs::current_path() / "output";
            spdlog::info("The ‘output’ parameter is not specified, used: {}", 
                result.output_dir.string());
        }
        
        // filename_mask (опциональный параметр)
        result.filename_mask = extract_string_array(main_table, "filename_mask");
        
        if (!result.is_valid()) {
            spdlog::error("The configuration is incorrect");
            return std::nullopt;
        }
        
        return result;
        
    } catch (const toml::parse_error& err) {
        spdlog::error("Parsing error TOML: {}", err.what());
        return std::nullopt;
    } catch (const std::exception& err) {
        spdlog::error("Unexpected error while parsing configuration: {}", 
            err.what());
        return std::nullopt;
    }
}

std::optional<fs::path> config_parser::extract_path(
    const toml::table& table_,
    const std::string& key_) noexcept {
    
    try {
        if (auto value = table_[key_].value<std::string>()) {
            return fs::path(*value);
        }
        return std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<std::string> config_parser::extract_string_array(
    const toml::table& table_,
    const std::string& key_) noexcept {
    
    try {
        std::vector<std::string> result;
        
        if (auto arr = table_[key_].as_array()) {
            for (const auto& elem : *arr) {
                if (auto str = elem.value<std::string>()) {
                    result.push_back(*str);
                }
            }
        }
        
        return result;
    } catch (...) {
        return {};
    }
}

#endif  // CONFIG_PARSER_HPP