/**
 * \file csv_helpers.hpp
 * \author github:PsiXich
 * \brief Вспомогательные функции для парсинга CSV
 * \date 2025-02-23
 * \version 2.1.1
 */

#ifndef CSV_HELPERS_HPP
#define CSV_HELPERS_HPP

#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <cstdint>
#include "csv_record.hpp"

/**
 * \brief Namespace с утилитами для работы с CSV
 */
namespace csv_helpers {

/**
 * \brief Парсинг строки CSV в вектор полей
 * \param line_ строка CSV
 * \param delimiter_ разделитель (по умолчанию ';')
 * \return вектор полей
 */
[[nodiscard]] inline std::vector<std::string> split_line(
    const std::string& line_,
    char delimiter_ = ';') noexcept {
    
    std::vector<std::string> result;
    std::stringstream ss(line_);
    std::string field;
    
    while (std::getline(ss, field, delimiter_)) {
        // Удаление пробелов
        const auto start = field.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            result.push_back("");
        } else {
            const auto end = field.find_last_not_of(" \t\r\n");
            result.push_back(field.substr(start, end - start + 1));
        }
    }
    
    return result;
}

/**
 * \brief Определение типа CSV файла по заголовку
 * \param header_fields_ поля заголовка
 * \return true если level.csv, false если trade.csv
 */
[[nodiscard]] inline bool is_level_file(
    const std::vector<std::string>& header_fields_) noexcept {
    
    // level.csv содержит поле "rebuild"
    for (const auto& field : header_fields_) {
        if (field == "rebuild") {
            return true;
        }
    }
    return false;
}

/**
 * \brief Парсинг строки в level_record
 * \param fields_ поля строки
 * \return опциональная запись (nullopt при ошибке)
 */
[[nodiscard]] inline std::optional<level_record> parse_level_record(
    const std::vector<std::string>& fields_) noexcept {
    
    // Ожидаем 6 полей: receive_ts|exchange_ts|price|quantity|side|rebuild
    if (fields_.size() != 6) {
        return std::nullopt;
    }
    
    level_record record;
    
    // Парсинг receive_ts
    try {
        if (fields_[0].empty()) return std::nullopt;
        record.receive_ts = std::stoull(fields_[0]);
    } catch (...) {
        return std::nullopt;
    }
    
    // Парсинг exchange_ts
    try {
        if (fields_[1].empty()) return std::nullopt;
        record.exchange_ts = std::stoull(fields_[1]);
    } catch (...) {
        return std::nullopt;
    }
    
    // Парсинг price
    try {
        if (fields_[2].empty()) return std::nullopt;
        record.price = std::stod(fields_[2]);
    } catch (...) {
        return std::nullopt;
    }
    
    // Парсинг quantity
    try {
        if (fields_[3].empty()) return std::nullopt;
        record.quantity = std::stod(fields_[3]);
    } catch (...) {
        return std::nullopt;
    }
    
    // side - строка
    record.side = fields_[4];
    
    // Парсинг rebuild
    try {
        if (fields_[5].empty()) return std::nullopt;
        record.rebuild = std::stoi(fields_[5]);
    } catch (...) {
        return std::nullopt;
    }
    
    return record;
}

/**
 * \brief Парсинг строки в trade_record
 * \param fields_ поля строки
 * \return опциональная запись (nullopt при ошибке)
 */
[[nodiscard]] inline std::optional<trade_record> parse_trade_record(
    const std::vector<std::string>& fields_) noexcept {
    
    // Ожидаем 5 полей: receive_ts|exchange_ts|price|quantity|side
    if (fields_.size() != 5) {
        return std::nullopt;
    }
    
    trade_record record;
    
    // Парсинг receive_ts
    try {
        if (fields_[0].empty()) return std::nullopt;
        record.receive_ts = std::stoull(fields_[0]);
    } catch (...) {
        return std::nullopt;
    }
    
    // Парсинг exchange_ts
    try {
        if (fields_[1].empty()) return std::nullopt;
        record.exchange_ts = std::stoull(fields_[1]);
    } catch (...) {
        return std::nullopt;
    }
    
    // Парсинг price
    try {
        if (fields_[2].empty()) return std::nullopt;
        record.price = std::stod(fields_[2]);
    } catch (...) {
        return std::nullopt;
    }
    
    // Парсинг quantity
    try {
        if (fields_[3].empty()) return std::nullopt;
        record.quantity = std::stod(fields_[3]);
    } catch (...) {
        return std::nullopt;
    }
    
    // side - строка
    record.side = fields_[4];
    
    return record;
}

} // namespace csv_helpers

#endif  // CSV_HELPERS_HPP