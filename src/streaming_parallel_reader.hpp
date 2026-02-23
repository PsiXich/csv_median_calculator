/**
 * \file streaming_parallel_reader.hpp
 * \author github:PsiXich
 * \brief Потоковый параллельный читатель для файлов > RAM
 * \date 2025-02-23
 * \version 2.1
 */

#ifndef STREAMING_PARALLEL_READER_HPP
#define STREAMING_PARALLEL_READER_HPP

#include <filesystem>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <chrono>
#include <spdlog/spdlog.h>
#include "streaming_csv_reader.hpp"
#include "thread_safe_queue.hpp"
#include "csv_record.hpp"

namespace fs = std::filesystem;

/**
 * \brief Статистика потокового параллельного чтения
 */
struct streaming_parallel_stats {
    std::size_t total_records;
    std::size_t total_batches;
    std::size_t invalid_records;
    std::chrono::milliseconds total_time;
    std::size_t peak_memory_mb;
    
    streaming_parallel_stats() noexcept :
        total_records{0},
        total_batches{0},
        invalid_records{0},
        total_time{0},
        peak_memory_mb{0}
    {}
};

/**
 * \brief Параллельный потоковый читатель CSV файлов
 * 
 * Комбинирует многопоточное чтение с потоковой обработкой
 * для эффективной работы с очень большими файлами.
 */
class streaming_parallel_reader {
public:
    /**
     * \brief Конструктор
     * \param queue_ очередь для записи данных
     * \param config_ конфигурация потокового чтения
     */
    explicit streaming_parallel_reader(
        std::shared_ptr<thread_safe_queue<csv_record>> queue_,
        const streaming_config& config_ = {}) noexcept :
        _queue{queue_},
        _config{config_},
        _active_threads{0}
    {}
    
    /**
     * \brief Запретить копирование
     */
    streaming_parallel_reader(const streaming_parallel_reader&) = delete;
    streaming_parallel_reader& operator=(const streaming_parallel_reader&) = delete;
    
    /**
     * \brief Потоковое чтение файлов параллельно
     * \param files_ список файлов
     * \return статистика
     */
    [[nodiscard]] streaming_parallel_stats read_files_streaming(
        const std::vector<fs::path>& files_);

private:
    /**
     * \brief Рабочая функция потока для потокового чтения файла
     * \param file_path_ путь к файлу
     * \param stats_ статистика (выходной параметр)
     */
    void stream_file_worker(
        const fs::path& file_path_,
        streaming_stats& stats_) noexcept;

private:
    std::shared_ptr<thread_safe_queue<csv_record>> _queue;
    streaming_config _config;
    std::atomic<int> _active_threads;
};

// ============================================================================
// Реализация
// ============================================================================

streaming_parallel_stats streaming_parallel_reader::read_files_streaming(
    const std::vector<fs::path>& files_) {
    
    if (files_.empty()) {
        spdlog::warn("No files to read");
        return {};
    }
    
    spdlog::info("Starting STREAMING parallel reading of {} files...", 
        files_.size());
    spdlog::info("Batch size: {} records", _config.batch_size);
    spdlog::info("Memory limit: {} MB", _config.max_memory_mb);
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Векторы для потоков и статистики
    std::vector<std::thread> threads;
    std::vector<streaming_stats> file_stats(files_.size());
    
    threads.reserve(files_.size());
    
    // Запуск потока для каждого файла
    for (std::size_t i = 0; i < files_.size(); ++i) {
        _active_threads.fetch_add(1, std::memory_order_relaxed);
        
        threads.emplace_back([this, &files_, &file_stats, i]() {
            this->stream_file_worker(files_[i], file_stats[i]);
            _active_threads.fetch_sub(1, std::memory_order_relaxed);
        });
    }
    
    // Ожидание завершения всех потоков
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    // Сигнал о завершении
    _queue->finish();
    
    auto end_time = std::chrono::steady_clock::now();
    
    // Сводная статистика
    streaming_parallel_stats total_stats;
    total_stats.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    for (const auto& stats : file_stats) {
        total_stats.total_records += stats.total_records;
        total_stats.total_batches += stats.batches_processed;
        total_stats.invalid_records += stats.invalid_records;
    }
    
    // Вывод сводной статистики
    spdlog::info("Streaming parallel reading completed:");
    spdlog::info("  Files processed: {}", files_.size());
    spdlog::info("  Total records: {}", total_stats.total_records);
    spdlog::info("  Total batches: {}", total_stats.total_batches);
    spdlog::info("  Time: {} ms", total_stats.total_time.count());
    
    if (total_stats.total_time.count() > 0) {
        double records_per_sec = (total_stats.total_records * 1000.0) / 
            total_stats.total_time.count();
        spdlog::info("  Throughput: {:.0f} records/sec", records_per_sec);
    }
    
    if (total_stats.invalid_records > 0) {
        spdlog::warn("  Invalid records: {}", total_stats.invalid_records);
    }
    
    return total_stats;
}

void streaming_parallel_reader::stream_file_worker(
    const fs::path& file_path_,
    streaming_stats& stats_) noexcept {
    
    try {
        streaming_csv_reader reader(_config);
        
        // Callback для обработки каждого батча
        auto batch_callback = [this](std::vector<csv_record>&& batch) -> bool {
            // Добавляем все записи из батча в очередь
            for (auto& record : batch) {
                _queue->push(std::move(record));
            }
            return true; // Продолжить чтение
        };
        
        // Потоковое чтение файла
        stats_ = reader.read_file_streaming(file_path_, batch_callback);
        
    } catch (const std::exception& err) {
        spdlog::error("Error in streaming worker for {}: {}", 
            file_path_.filename().string(), err.what());
    }
}

#endif  // STREAMING_PARALLEL_READER_HPP