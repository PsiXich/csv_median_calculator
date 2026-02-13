/**
 * \file csv_reader.hpp
 * \author github:PsiXich
 * \brief Парсер CSV файлов с биржевыми данными
 * \date 2025-02-13
 * \version 1.1
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
     * \brief Парсинг строки CSV в вектор полей
     * \param line_ строка CSV
     * \param delimiter_ разделитель (по умолчанию ';')
     * \return вектор полей
     */
    [[nodiscard]] static std::vector<std::string> split_line(
        const std::string& line_,
        char delimiter_ = ';') noexcept;
    
    /**
     * \brief Определение типа CSV файла по заголовку
     * \param header_fields_ поля заголовка
     * \return true если level.csv, false если trade.csv
     */
    [[nodiscard]] static bool is_level_file(
        const std::vector<std::string>& header_fields_) noexcept;
    
    /**
     * \brief Парсинг строки в level_record
     * \param fields_ поля строки
     * \return опциональная запись (nullopt при ошибке)
     */
    [[nodiscard]] static std::optional<level_record> parse_level_record(
        const std::vector<std::string>& fields_) noexcept;
    
    /**
     * \brief Парсинг строки в trade_record
     * \param fields_ поля строки
     * \return опциональная запись (nullopt при ошибке)
     */
    [[nodiscard]] static std::optional<trade_record> parse_trade_record(
        const std::vector<std::string>& fields_) noexcept;
    
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
            spdlog::error("Не удалось открыть файл: {}", 
                file_path_.string());
            return result;
        }
        
        std::string line;
        std::size_t line_number = 0;
        
        // Чтение заголовка
        if (!std::getline(file, line)) {
            spdlog::error("Файл пуст: {}", file_path_.string());
            return result;
        }
        line_number++;
        
        auto header_fields = split_line(line);
        if (header_fields.empty()) {
            spdlog::error("Некорректный заголовок в файле: {}", 
                file_path_.string());
            return result;
        }
        
        // Определение типа файла
        const bool is_level = is_level_file(header_fields);
        
        // Чтение данных
        while (std::getline(file, line)) {
            line_number++;
            
            // Пропуск пустых строк
            if (trim(line).empty()) {
                continue;
            }
            
            auto fields = split_line(line);
            
            if (is_level) {
                auto record = parse_level_record(fields);
                if (record) {
                    result.push_back(*record);
                } else {
                    spdlog::warn("Ошибка парсинга строки {} в файле {}", 
                        line_number, file_path_.filename().string());
                }
            } else {
                auto record = parse_trade_record(fields);
                if (record) {
                    result.push_back(*record);
                } else {
                    spdlog::warn("Ошибка парсинга строки {} в файле {}", 
                        line_number, file_path_.filename().string());
                }
            }
        }
        
        spdlog::info("Прочитано {} записей из файла {}", 
            result.size(), file_path_.filename().string());
        
    } catch (const std::exception& err) {
        spdlog::error("Ошибка чтения файла {}: {}", 
            file_path_.string(), err.what());
    }
    
    return result;
}

std::vector<std::string> csv_reader::split_line(
    const std::string& line_,
    char delimiter_) noexcept {
    
    std::vector<std::string> result;
    std::stringstream ss(line_);
    std::string field;
    
    while (std::getline(ss, field, delimiter_)) {
        result.push_back(trim(field));
    }
    
    return result;
}

bool csv_reader::is_level_file(
    const std::vector<std::string>& header_fields_) noexcept {
    
    // level.csv содержит поле "rebuild"
    for (const auto& field : header_fields_) {
        if (field == "rebuild") {
            return true;
        }
    }
    return false;
}

std::optional<level_record> csv_reader::parse_level_record(
    const std::vector<std::string>& fields_) noexcept {
    
    // Ожидаем 6 полей: receive_ts|exchange_ts|price|quantity|side|rebuild
    if (fields_.size() != 6) {
        return std::nullopt;
    }
    
    level_record record;
    
    auto receive_ts = parse_uint64(fields_[0]);
    if (!receive_ts) return std::nullopt;
    record.receive_ts = *receive_ts;
    
    auto exchange_ts = parse_uint64(fields_[1]);
    if (!exchange_ts) return std::nullopt;
    record.exchange_ts = *exchange_ts;
    
    auto price = parse_double(fields_[2]);
    if (!price) return std::nullopt;
    record.price = *price;
    
    auto quantity = parse_double(fields_[3]);
    if (!quantity) return std::nullopt;
    record.quantity = *quantity;
    
    record.side = fields_[4];
    
    auto rebuild = parse_int(fields_[5]);
    if (!rebuild) return std::nullopt;
    record.rebuild = *rebuild;
    
    return record;
}

std::optional<trade_record> csv_reader::parse_trade_record(
    const std::vector<std::string>& fields_) noexcept {
    
    // Ожидаем 5 полей: receive_ts|exchange_ts|price|quantity|side
    if (fields_.size() != 5) {
        return std::nullopt;
    }
    
    trade_record record;
    
    auto receive_ts = parse_uint64(fields_[0]);
    if (!receive_ts) return std::nullopt;
    record.receive_ts = *receive_ts;
    
    auto exchange_ts = parse_uint64(fields_[1]);
    if (!exchange_ts) return std::nullopt;
    record.exchange_ts = *exchange_ts;
    
    auto price = parse_double(fields_[2]);
    if (!price) return std::nullopt;
    record.price = *price;
    
    auto quantity = parse_double(fields_[3]);
    if (!quantity) return std::nullopt;
    record.quantity = *quantity;
    
    record.side = fields_[4];
    
    return record;
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