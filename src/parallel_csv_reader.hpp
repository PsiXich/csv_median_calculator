/**
 * \file parallel_csv_reader.hpp
 * \author github:PsiXich
 * \brief Параллельное чтение CSV файлов
 * \date 2025-02-23
 * \version 2.1
 */

#ifndef PARALLEL_CSV_READER_HPP
#define PARALLEL_CSV_READER_HPP

#include <filesystem>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <chrono>
#include <sstream>
#include <spdlog/spdlog.h>
#include "csv_reader.hpp"
#include "csv_record.hpp"
#include "thread_safe_queue.hpp"

namespace fs = std::filesystem;

/**
 * \brief Статистика чтения одного файла
 */
struct read_stats {
    fs::path file_path;              ///< Путь к файлу
    std::size_t records_read;        ///< Прочитано записей
    std::chrono::milliseconds time;  ///< Время чтения
    bool success;                    ///< Успешность чтения
    
    read_stats() noexcept : 
        records_read{0}, 
        time{0}, 
        success{false} 
    {}
};

/**
 * \brief Класс для параллельного чтения CSV файлов
 * 
 * Создаёт поток для каждого файла, читает данные параллельно
 * и складывает в потокобезопасную очередь.
 */
class parallel_csv_reader {
public:
    /**
     * \brief Конструктор
     * \param queue_ потокобезопасная очередь для записи данных
     */
    explicit parallel_csv_reader(
        std::shared_ptr<thread_safe_queue<csv_record>> queue_) noexcept :
        _queue{queue_},
        _active_threads{0}
    {}
    
    /**
     * \brief Запретить копирование
     */
    parallel_csv_reader(const parallel_csv_reader&) = delete;
    parallel_csv_reader& operator=(const parallel_csv_reader&) = delete;
    
    /**
     * \brief Прочитать файлы параллельно
     * \param files_ список файлов для чтения
     * \return вектор статистики по каждому файлу
     */
    [[nodiscard]] std::vector<read_stats> read_files(
        const std::vector<fs::path>& files_) {
        
        if (files_.empty()) {
            spdlog::warn("No files to read");
            return {};
        }
        
        spdlog::info("Starting parallel reading of {} files...", files_.size());
        
        // Вектор потоков
        std::vector<std::thread> threads;
        threads.reserve(files_.size());
        
        // Вектор для статистики индексированный
        std::vector<read_stats> stats(files_.size());
        
        // Запуск потока для каждого файла
        for (std::size_t i = 0; i < files_.size(); ++i) {
            _active_threads.fetch_add(1, std::memory_order_relaxed);
            
            threads.emplace_back([this, &files_, &stats, i]() {
                this->read_file_worker(files_[i], stats[i]);
                _active_threads.fetch_sub(1, std::memory_order_relaxed);
            });
        }
        
        // Ожидание завершения всех потоков
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        // Сигнализируем что все файлы прочитаны
        _queue->finish();
        
        // Вывод статистики
        print_statistics(stats);
        
        return stats;
    }
    
    /**
     * \brief Получить количество активных потоков чтения
     * \return количество активных потоков
     */
    [[nodiscard]] int get_active_threads() const noexcept {
        return _active_threads.load(std::memory_order_relaxed);
    }

private:
    /**
     * \brief Рабочая функция потока для чтения одного файла
     * \param file_path_ путь к файлу
     * \param stats_ статистика (выходной параметр)
     */
    void read_file_worker(const fs::path& file_path_, read_stats& stats_) noexcept {
        stats_.file_path = file_path_;
        
        auto start_time = std::chrono::steady_clock::now();
        
        try {
            // Преобразуем thread::id в строку для логирования
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            spdlog::debug("Thread {} reading file: {}", 
                oss.str(), 
                file_path_.filename().string());
            
            // Читаем файл (используем существующий csv_reader)
            auto records = csv_reader::read_file(file_path_);
            
            if (records.empty()) {
                spdlog::warn("File {} is empty or failed to read", 
                    file_path_.filename().string());
                stats_.success = false;
                return;
            }
            
            // Добавляем все записи в очередь
            for (auto& record : records) {
                _queue->push(std::move(record));
            }
            
            stats_.records_read = records.size();
            stats_.success = true;
            
            auto end_time = std::chrono::steady_clock::now();
            stats_.time = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);
            
            // Преобразуем thread::id в строку для логирования
            std::ostringstream oss2;
            oss2 << std::this_thread::get_id();
            spdlog::debug("Thread {} finished reading {} records in {} ms", 
                oss2.str(),
                stats_.records_read,
                stats_.time.count());
            
        } catch (const std::exception& err) {
            spdlog::error("Error reading file {}: {}", 
                file_path_.filename().string(), 
                err.what());
            stats_.success = false;
        }
    }
    
    /**
     * \brief Вывод сводной статистики
     * \param stats_ вектор статистики по файлам
     */
    void print_statistics(const std::vector<read_stats>& stats_) const noexcept {
        std::size_t total_records = 0;
        std::chrono::milliseconds total_time{0};
        std::size_t successful_files = 0;
        
        for (const auto& stat : stats_) {
            if (stat.success) {
                total_records += stat.records_read;
                total_time = std::max(total_time, stat.time);
                successful_files++;
            }
        }
        
        spdlog::info("Parallel reading completed:");
        spdlog::info("  Files processed: {}/{}", successful_files, stats_.size());
        spdlog::info("  Total records: {}", total_records);
        spdlog::info("  Time (max): {} ms", total_time.count());
        
        if (total_time.count() > 0) {
            double records_per_sec = (total_records * 1000.0) / total_time.count();
            spdlog::info("  Throughput: {:.0f} records/sec", records_per_sec);
        }
    }

private:
    std::shared_ptr<thread_safe_queue<csv_record>> _queue;  ///< Очередь для записи
    std::atomic<int> _active_threads;                       ///< Счётчик активных потоков
};

#endif  // PARALLEL_CSV_READER_HPP