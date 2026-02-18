/**
 * \file median_calculator.hpp
 * \author github:PsiXich
 * \brief Расчет медианы с использованием Boost.Accumulators
 * \date 2025-02-17
 * \version 1.1
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
};

// ============================================================================
// Реализация
// ============================================================================

std::vector<median_result> median_calculator::calculate(
    const std::vector<csv_record>& records_) noexcept {
    
    std::vector<median_result> results;
    
    try {
        if (records_.empty()) {
            spdlog::warn("Нет данных для расчета медианы");
            return results;
        }
        
        accumulator_t acc;
        std::optional<double> previous_median;
        
        for (const auto& record : records_) {
            // Добавление цены в аккумулятор
            const double price = get_price(record);
            acc(price);
            
            // Вычисление текущей медианы
            const double current_median = acc::median(acc);
            
            // Запись результата только при изменении медианы
            if (!previous_median || *previous_median != current_median) {
                const auto receive_ts = get_receive_ts(record);
                results.emplace_back(receive_ts, current_median);
                previous_median = current_median;
            }
        }
        
        spdlog::info("Записано изменений медианы: {}", results.size());
        
    } catch (const std::exception& err) {
        spdlog::error("Ошибка при расчете медианы: {}", err.what());
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
            spdlog::error("Не удалось создать выходной файл: {}", 
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
            spdlog::error("Ошибка записи в файл: {}", 
                output_path_.string());
            return false;
        }
        
        spdlog::info("Результат сохранен: {}", output_path_.string());
        return true;
        
    } catch (const std::exception& err) {
        spdlog::error("Ошибка сохранения результатов: {}", err.what());
        return false;
    }
}

#endif  // MEDIAN_CALCULATOR_HPP