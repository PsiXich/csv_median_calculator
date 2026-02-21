/**
 * \file median_calculator.hpp
 * \author github:PsiXich
 * \brief Расчет медианы с использованием Boost.Accumulators
 * \date 2025-02-21
 * \version 1.3
 */

#ifndef MEDIAN_CALCULATOR_HPP
#define MEDIAN_CALCULATOR_HPP

#include <vector>
#include <optional>
#include <filesystem>
#include <fstream>
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/median.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <spdlog/spdlog.h>
#include "csv_record.hpp"

namespace fs = std::filesystem;
namespace acc = boost::accumulators;

/**
 * \brief Результат расчета медианы
 */
struct median_result {
    std::uint64_t receive_ts;  ///< Временная метка изменения медианы
    double price_median;       ///< Значение медианы
    
    /**
     * \brief Конструктор
     */
    median_result(std::uint64_t ts_, double median_) noexcept :
        receive_ts{ts_},
        price_median{median_}
    {}
};

/**
 * \brief Калькулятор медианы с инкрементальным обновлением
 */
class median_calculator {
public:
    /**
     * \brief Обработка записей и расчет медианы
     * \param records_ вектор отсортированных записей
     * \return вектор результатов (только изменения медианы)
     */
    [[nodiscard]] static std::vector<median_result> calculate(
        const std::vector<csv_record>& records_) noexcept;
    
    /**
     * \brief Сохранение результатов в CSV файл
     * \param results_ вектор результатов
     * \param output_path_ путь к выходному файлу
     * \return true при успехе
     */
    [[nodiscard]] static bool save_results(
        const std::vector<median_result>& results_,
        const fs::path& output_path_) noexcept;

private:
    /// Тип аккумулятора для расчета медианы
    using accumulator_t = acc::accumulator_set<
        double,
        acc::stats<acc::tag::median(acc::with_p_square_quantile)>
    >;
    
    /// Минимальное количество значений для корректной работы P² алгоритма
    static constexpr std::size_t MIN_VALUES_FOR_P2 = 5;
};

// ============================================================================
// Реализация
// ============================================================================

std::vector<median_result> median_calculator::calculate(
    const std::vector<csv_record>& records_) noexcept {
    
    std::vector<median_result> results;
    
    try {
        if (records_.empty()) {
            spdlog::warn("No data for median calculation");
            return results;
        }
        
        accumulator_t acc;
        std::optional<double> previous_median;
        std::vector<double> initial_values;  // Для первых значений
        
        for (std::size_t i = 0; i < records_.size(); ++i) {
            const auto& record = records_[i];
            const double price = get_price(record);
            const auto receive_ts = get_receive_ts(record);
            
            // Добавление цены в аккумулятор
            acc(price);
            
            // Для первых MIN_VALUES_FOR_P2 значений храним в векторе
            if (i < MIN_VALUES_FOR_P2) {
                initial_values.push_back(price);
                
                // Вычисляем медиану вручную для первых значений
                std::vector<double> sorted_values = initial_values;
                std::sort(sorted_values.begin(), sorted_values.end());
                
                double current_median;
                std::size_t n = sorted_values.size();
                if (n % 2 == 0) {
                    // Чётное количество - среднее двух центральных
                    current_median = (sorted_values[n/2 - 1] + sorted_values[n/2]) / 2.0;
                } else {
                    // Нечётное - центральный элемент
                    current_median = sorted_values[n/2];
                }
                
                // Запись результата только при изменении медианы
                if (!previous_median || *previous_median != current_median) {
                    results.emplace_back(receive_ts, current_median);
                    previous_median = current_median;
                }
            } else {
                // После MIN_VALUES_FOR_P2 используем P² алгоритм
                const double current_median = acc::median(acc);
                
                // Запись результата только при изменении медианы
                if (!previous_median || *previous_median != current_median) {
                    results.emplace_back(receive_ts, current_median);
                    previous_median = current_median;
                }
            }
        }
        
        spdlog::info("Recorded median changes: {}", results.size());
        
    } catch (const std::exception& err) {
        spdlog::error("Error calculating median: {}", err.what());
    }
    
    return results;
}

bool median_calculator::save_results(
    const std::vector<median_result>& results_,
    const fs::path& output_path_) noexcept {
    
    try {
        // Создание директории если не существует
        const auto output_dir = output_path_.parent_path();
        if (!output_dir.empty() && !fs::exists(output_dir)) {
            fs::create_directories(output_dir);
        }
        
        std::ofstream file(output_path_);
        if (!file.is_open()) {
            spdlog::error("Failed to create output file: {}", 
                output_path_.string());
            return false;
        }
        
        // Запись заголовка
        file << "receive_ts;price_median\n";
        
        // Запись данных с форматированием
        file << std::fixed;
        file.precision(8);
        
        for (const auto& result : results_) {
            file << result.receive_ts << ";" 
                 << result.price_median << "\n";
        }
        
        file.close();
        
        if (!file.good()) {
            spdlog::error("Error writing to file: {}", 
                output_path_.string());
            return false;
        }
        
        spdlog::info("Results saved: {}", output_path_.string());
        return true;
        
    } catch (const std::exception& err) {
        spdlog::error("Error saving results: {}", err.what());
        return false;
    }
}

#endif  // MEDIAN_CALCULATOR_HPP