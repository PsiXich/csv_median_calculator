/**
 * \file csv_reader.hpp
 * \author github:PsiXich
 * \brief Парсер CSV файлов с биржевыми данными
 * \date 2025-02-23
 * \version 2.0
 */

#ifndef CSV_READER_HPP
#define CSV_READER_HPP

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <optional>
#include <spdlog/spdlog.h>
#include "csv_record.hpp"
#include "csv_reader_helper.hpp"

namespace fs = std::filesystem;

/**
 * \brief Парсер CSV файлов
 */
class csv_reader {
public:
    /**
     * \brief Чтение всех записей из CSV файла
     * \param file_path_ путь к CSV файлу
     * \return вектор записей (пустой при ошибке)
     */
    [[nodiscard]] static std::vector<csv_record> read_file(
        const fs::path& file_path_) noexcept;

private:   
    /**
     * \brief Безопасный парсинг uint64_t
     * \param str_ строка для парсинга
     * \return опциональное значение
     */
    [[nodiscard]] static std::optional<std::uint64_t> parse_uint64(
        const std::string& str_) noexcept;
    
    /**
     * \brief Безопасный парсинг double
     * \param str_ строка для парсинга
     * \return опциональное значение
     */
    [[nodiscard]] static std::optional<double> parse_double(
        const std::string& str_) noexcept;
    
    /**
     * \brief Безопасный парсинг int
     * \param str_ строка для парсинга
     * \return опциональное значение
     */
    [[nodiscard]] static std::optional<int> parse_int(
        const std::string& str_) noexcept;
    
    /**
     * \brief Удаление пробелов в начале и конце строки
     * \param str_ строка
     * \return обрезанная строка
     */
    [[nodiscard]] static std::string trim(const std::string& str_) noexcept;
};

// ============================================================================
// Реализация
// ============================================================================

std::vector<csv_record> csv_reader::read_file(
    const fs::path& file_path_) noexcept {
    
    std::vector<csv_record> result;
    
    try {
        std::ifstream file(file_path_);
        if (!file.is_open()) {
            spdlog::error("Unable to open file: {}", 
                file_path_.string());
            return result;
        }
        
        std::string line;
        std::size_t line_number = 0;
        
        // Чтение заголовка
        if (!std::getline(file, line)) {
            spdlog::error("The file is empty: {}", file_path_.string());
            return result;
        }
        line_number++;
        
        auto header_fields = csv_helpers::split_line(line);
        if (header_fields.empty()) {
            spdlog::error("Incorrect header in file: {}", 
                file_path_.string());
            return result;
        }
        
        // Определение типа файла
        const bool is_level = csv_helpers::is_level_file(header_fields);
        
        // Чтение данных
        while (std::getline(file, line)) {
            line_number++;
            
            // Пропуск пустых строк
            if (trim(line).empty()) {
                continue;
            }
            
            auto fields = csv_helpers::split_line(line);
            
            if (is_level) {
                auto record = csv_helpers::parse_level_record(fields);
                if (record) {
                    result.push_back(*record);
                } else {
                    spdlog::warn("String parsing error {} in the file {}", 
                        line_number, file_path_.filename().string());
                }
            } else {
                auto record = csv_helpers::parse_trade_record(fields);
                if (record) {
                    result.push_back(*record);
                } else {
                    spdlog::warn("String parsing error {} in the file {}", 
                        line_number, file_path_.filename().string());
                }
            }
        }
        
        spdlog::info("Read {} records from the file {}", 
            result.size(), file_path_.filename().string());
        
    } catch (const std::exception& err) {
        spdlog::error("File read error {}: {}", 
            file_path_.string(), err.what());
    }
    
    return result;
}

std::optional<std::uint64_t> csv_reader::parse_uint64(
    const std::string& str_) noexcept {
    
    try {
        if (str_.empty()) {
            return std::nullopt;
        }
        return std::stoull(str_);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> csv_reader::parse_double(
    const std::string& str_) noexcept {
    
    try {
        if (str_.empty()) {
            return std::nullopt;
        }
        return std::stod(str_);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> csv_reader::parse_int(
    const std::string& str_) noexcept {
    
    try {
        if (str_.empty()) {
            return std::nullopt;
        }
        return std::stoi(str_);
    } catch (...) {
        return std::nullopt;
    }
}

std::string csv_reader::trim(const std::string& str_) noexcept {
    const auto start = str_.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    
    const auto end = str_.find_last_not_of(" \t\r\n");
    return str_.substr(start, end - start + 1);
}

#endif  // CSV_READER_HPP