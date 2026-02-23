/**
 * \file streaming_csv_reader.hpp
 * \author github:PsiXich
 * \brief Потоковый читатель CSV файлов для обработки файлов > RAM
 * \date 2025-02-23
 * \version 2.1
 */

#ifndef STREAMING_CSV_READER_HPP
#define STREAMING_CSV_READER_HPP

#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <spdlog/spdlog.h>
#include "csv_record.hpp"

namespace fs = std::filesystem;

/**
 * \brief Конфигурация потокового чтения
 */
struct streaming_config {
    std::size_t batch_size;         ///< Размер батча (количество записей)
    std::size_t max_memory_mb;      ///< Максимальное использование памяти (MB)
    bool skip_invalid_records;      ///< Пропускать некорректные записи
    
    /**
     * \brief Конструктор с настройками по умолчанию
     */
    streaming_config() noexcept :
        batch_size{10000},          // 10k записей на батч
        max_memory_mb{512},         // 512 MB лимит памяти
        skip_invalid_records{true}
    {}
};

/**
 * \brief Статистика потокового чтения
 */
struct streaming_stats {
    std::size_t total_records;      ///< Всего прочитано записей
    std::size_t invalid_records;    ///< Количество некорректных записей
    std::size_t batches_processed;  ///< Количество обработанных батчей
    std::size_t bytes_read;         ///< Байт прочитано
    
    streaming_stats() noexcept :
        total_records{0},
        invalid_records{0},
        batches_processed{0},
        bytes_read{0}
    {}
};

/**
 * \brief Потоковый читатель CSV файлов
 * 
 * Читает файл батчами, не загружая весь файл в память.
 * Идеально для файлов больше доступной RAM.
 */
class streaming_csv_reader {
public:
    /**
     * \brief Callback функция для обработки батча записей
     * \param batch вектор записей в батче
     * \return true для продолжения чтения, false для остановки
     */
    using batch_callback = std::function<bool(std::vector<csv_record>&&)>;
    
    /**
     * \brief Конструктор
     * \param config_ конфигурация потокового чтения
     */
    explicit streaming_csv_reader(const streaming_config& config_ = {}) noexcept :
        _config{config_},
        _is_level_file{false}
    {}
    
    /**
     * \brief Потоковое чтение файла с callback на каждый батч
     * \param file_path_ путь к файлу
     * \param callback_ функция обработки батча
     * \return статистика чтения
     */
    [[nodiscard]] streaming_stats read_file_streaming(
        const fs::path& file_path_,
        batch_callback callback_) noexcept;

private:
    /**
     * \brief Парсинг строки CSV в вектор полей
     * \param line_ строка CSV
     * \param delimiter_ разделитель
     * \return вектор полей
     */
    [[nodiscard]] static std::vector<std::string> split_line(
        const std::string& line_,
        char delimiter_ = ';') noexcept;
    
    /**
     * \brief Определение типа файла по заголовку
     * \param header_fields_ поля заголовка
     * \return true если level.csv
     */
    [[nodiscard]] static bool is_level_file(
        const std::vector<std::string>& header_fields_) noexcept;
    
    /**
     * \brief Парсинг строки в level_record
     * \param fields_ поля строки
     * \return опциональная запись
     */
    [[nodiscard]] static std::optional<level_record> parse_level_record(
        const std::vector<std::string>& fields_) noexcept;
    
    /**
     * \brief Парсинг строки в trade_record
     * \param fields_ поля строки
     * \return опциональная запись
     */
    [[nodiscard]] static std::optional<trade_record> parse_trade_record(
        const std::vector<std::string>& fields_) noexcept;
    
    /**
     * \brief Безопасный парсинг uint64_t
     */
    [[nodiscard]] static std::optional<std::uint64_t> parse_uint64(
        const std::string& str_) noexcept;
    
    /**
     * \brief Безопасный парсинг double
     */
    [[nodiscard]] static std::optional<double> parse_double(
        const std::string& str_) noexcept;
    
    /**
     * \brief Безопасный парсинг int
     */
    [[nodiscard]] static std::optional<int> parse_int(
        const std::string& str_) noexcept;
    
    /**
     * \brief Удаление пробелов
     */
    [[nodiscard]] static std::string trim(const std::string& str_) noexcept;

private:
    streaming_config _config;   ///< Конфигурация
    bool _is_level_file;        ///< Тип файла (level/trade)
};

// ============================================================================
// Реализация
// ============================================================================

streaming_stats streaming_csv_reader::read_file_streaming(
    const fs::path& file_path_,
    batch_callback callback_) noexcept {
    
    streaming_stats stats;
    
    try {
        std::ifstream file(file_path_, std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for streaming: {}", 
                file_path_.string());
            return stats;
        }
        
        // Получение размера файла
        file.seekg(0, std::ios::end);
        const auto file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        spdlog::info("Streaming file: {} ({} MB)", 
            file_path_.filename().string(),
            file_size / (1024 * 1024));
        
        std::string line;
        std::size_t line_number = 0;
        
        // Чтение заголовка
        if (!std::getline(file, line)) {
            spdlog::error("Empty file: {}", file_path_.string());
            return stats;
        }
        line_number++;
        
        auto header_fields = split_line(line);
        _is_level_file = is_level_file(header_fields);
        
        // Батч для накопления записей
        std::vector<csv_record> batch;
        batch.reserve(_config.batch_size);
        
        // Чтение данных батчами
        while (std::getline(file, line)) {
            line_number++;
            stats.bytes_read += line.size() + 1; // +1 для '\n'
            
            // Пропуск пустых строк
            if (trim(line).empty()) {
                continue;
            }
            
            auto fields = split_line(line);
            
            // Парсинг записи
            std::optional<csv_record> record_opt;
            
            if (_is_level_file) {
                if (auto rec = parse_level_record(fields)) {
                    record_opt = *rec;
                }
            } else {
                if (auto rec = parse_trade_record(fields)) {
                    record_opt = *rec;
                }
            }
            
            if (record_opt) {
                batch.push_back(std::move(*record_opt));
                stats.total_records++;
            } else {
                stats.invalid_records++;
                if (!_config.skip_invalid_records) {
                    spdlog::warn("Invalid record at line {} in {}", 
                        line_number, file_path_.filename().string());
                }
            }
            
            // Обработка батча при достижении размера
            if (batch.size() >= _config.batch_size) {
                stats.batches_processed++;
                
                // Вызов callback
                if (!callback_(std::move(batch))) {
                    spdlog::info("Streaming stopped by callback");
                    break;
                }
                
                // Очистка батча для следующей порции
                batch.clear();
                batch.reserve(_config.batch_size);
                
                // Периодический вывод прогресса
                if (stats.batches_processed % 10 == 0) {
                    const auto progress = (stats.bytes_read * 100) / file_size;
                    spdlog::debug("Streaming progress: {}% ({} records)", 
                        progress, stats.total_records);
                }
            }
        }
        
        // Обработка последнего батча (если есть остаток)
        if (!batch.empty()) {
            stats.batches_processed++;
            callback_(std::move(batch));
        }
        
        spdlog::info("Streaming completed: {} records in {} batches", 
            stats.total_records, stats.batches_processed);
        
        if (stats.invalid_records > 0) {
            spdlog::warn("Invalid records skipped: {}", stats.invalid_records);
        }
        
    } catch (const std::exception& err) {
        spdlog::error("Streaming error: {}", err.what());
    }
    
    return stats;
}

// ============================================================================
// Вспомогательные функции (копия из csv_reader.hpp)
// ============================================================================
// TODO: Refactor to use csv_helpers.hpp (eliminate code duplication)

std::vector<std::string> streaming_csv_reader::split_line(
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

bool streaming_csv_reader::is_level_file(
    const std::vector<std::string>& header_fields_) noexcept {
    
    for (const auto& field : header_fields_) {
        if (field == "rebuild") {
            return true;
        }
    }
    return false;
}

std::optional<level_record> streaming_csv_reader::parse_level_record(
    const std::vector<std::string>& fields_) noexcept {
    
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

std::optional<trade_record> streaming_csv_reader::parse_trade_record(
    const std::vector<std::string>& fields_) noexcept {
    
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

std::optional<std::uint64_t> streaming_csv_reader::parse_uint64(
    const std::string& str_) noexcept {
    
    try {
        if (str_.empty()) return std::nullopt;
        return std::stoull(str_);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> streaming_csv_reader::parse_double(
    const std::string& str_) noexcept {
    
    try {
        if (str_.empty()) return std::nullopt;
        return std::stod(str_);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> streaming_csv_reader::parse_int(
    const std::string& str_) noexcept {
    
    try {
        if (str_.empty()) return std::nullopt;
        return std::stoi(str_);
    } catch (...) {
        return std::nullopt;
    }
}

std::string streaming_csv_reader::trim(const std::string& str_) noexcept {
    const auto start = str_.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    
    const auto end = str_.find_last_not_of(" \t\r\n");
    return str_.substr(start, end - start + 1);
}

#endif  // STREAMING_CSV_READER_HPP