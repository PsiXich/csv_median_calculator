/**
 * \file data_processor.hpp
 * \author github:PsiXich
 * \brief Обработчик данных из потокобезопасной очереди
 * \date 2025-02-22
 * \version 2.0
 */

#ifndef DATA_PROCESSOR_HPP
#define DATA_PROCESSOR_HPP

#include <vector>
#include <memory>
#include <thread>
#include <algorithm>
#include <chrono>
#include <spdlog/spdlog.h>
#include "csv_record.hpp"
#include "thread_safe_queue.hpp"
#include "median_calculator.hpp"

/**
 * \brief Обработчик данных из очереди
 * 
 * Читает данные из потокобезопасной очереди,
 * собирает их, сортирует и передаёт на расчёт медианы.
 */
class data_processor {
public:
    /**
     * \brief Конструктор
     * \param queue_ очередь для чтения данных
     */
    explicit data_processor(
        std::shared_ptr<thread_safe_queue<csv_record>> queue_) noexcept :
        _queue{queue_}
    {}
    
    /**
     * \brief Запретить копирование
     */
    data_processor(const data_processor&) = delete;
    data_processor& operator=(const data_processor&) = delete;
    
    /**
     * \brief Обработать данные из очереди
     * \return вектор отсортированных записей
     * 
     * Читает все данные из очереди до её завершения,
     * собирает в вектор и сортирует по receive_ts.
     */
    [[nodiscard]] std::vector<csv_record> process() {
        spdlog::info("Starting data processing from queue...");
        
        auto start_time = std::chrono::steady_clock::now();
        
        std::vector<csv_record> all_records;
        
        // Читаем из очереди пока она не завершится
        std::size_t batch_count = 0;
        while (true) {
            auto record_opt = _queue->pop();
            
            if (!record_opt) {
                // Очередь завершена и пуста
                break;
            }
            
            all_records.push_back(std::move(*record_opt));
            
            // Периодический вывод прогресса
            if (++batch_count % 10000 == 0) {
                spdlog::debug("Processed {} records...", batch_count);
            }
        }
        
        spdlog::info("Collected {} records from queue", all_records.size());
        
        // Сортировка по receive_ts
        if (!all_records.empty()) {
            spdlog::info("Sorting data by timestamp...");
            
            auto sort_start = std::chrono::steady_clock::now();
            
            std::sort(all_records.begin(), all_records.end(),
                [](const csv_record& a_, const csv_record& b_) {
                    return get_receive_ts(a_) < get_receive_ts(b_);
                });
            
            auto sort_end = std::chrono::steady_clock::now();
            auto sort_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                sort_end - sort_start);
            
            spdlog::info("Sorting completed in {} ms", sort_time.count());
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        spdlog::info("Data processing completed in {} ms", total_time.count());
        
        return all_records;
    }
    
    /**
     * \brief Обработать данные и сразу рассчитать медиану
     * \return вектор результатов расчёта медианы
     * 
     * Полный цикл: чтение из очереди → сортировка → расчёт медианы
     */
    [[nodiscard]] std::vector<median_result> process_and_calculate() {
        // Собираем и сортируем данные
        auto all_records = process();
        
        if (all_records.empty()) {
            spdlog::warn("No data to calculate median");
            return {};
        }
        
        // Рассчитываем медиану
        spdlog::info("Starting median calculation...");
        auto results = median_calculator::calculate(all_records);
        
        return results;
    }

private:
    std::shared_ptr<thread_safe_queue<csv_record>> _queue;  ///< Очередь для чтения
};

#endif  // DATA_PROCESSOR_HPP