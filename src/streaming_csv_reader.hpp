/**
 * \file streaming_csv_reader.hpp
 * \author github:PsiXich
 * \brief Потоковый читатель CSV файлов для обработки файлов > RAM
 * \date 2025-02-25
 * \version 2.1.1
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
#include "csv_reader_helper.hpp"

namespace fs = std::filesystem;

/**
 * \brief Конфигурация потокового чтения
 */
struct streaming_config {
    std::size_t batch_size;         ///< Размер батча (количество записей)
    std::size_t max_memory_mb;      ///< Максимальное использование памяти (MB)
    bool skip_invalid_records;      ///< Пропускать некорректные записи
    
    streaming_config() noexcept :
        batch_size{10000},
        max_memory_mb{512},
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
 * Читает файл батчами, не загружая весь файл в память
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
        
        // Используем функцию из csv_helpers
        auto header_fields = csv_helpers::split_line(line);
        _is_level_file = csv_helpers::is_level_file(header_fields);
        
        // Батч для накопления записей
        std::vector<csv_record> batch;
        batch.reserve(_config.batch_size);
        
        // Чтение данных батчами
        while (std::getline(file, line)) {
            line_number++;
            stats.bytes_read += line.size() + 1;
            
            // Пропуск пустых строк
            auto trimmed = line;
            const auto start = trimmed.find_first_not_of(" \t\r\n");
            if (start == std::string::npos || trimmed.empty()) {
                continue;
            }
            
            // Используем функцию из csv_helpers
            auto fields = csv_helpers::split_line(line);
            
            // Парсинг записи (используем функции из csv_helpers)
            std::optional<csv_record> record_opt;
            
            if (_is_level_file) {
                if (auto rec = csv_helpers::parse_level_record(fields)) {
                    record_opt = *rec;
                }
            } else {
                if (auto rec = csv_helpers::parse_trade_record(fields)) {
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

#endif  // STREAMING_CSV_READER_HPP