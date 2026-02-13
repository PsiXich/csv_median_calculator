/**
 * \file csv_record.hpp
 * \author github:PsiXich
 * \brief Структуры данных для CSV записей
 * \date 2025-02-13
 * \version 1.1
 */

#ifndef CSV_RECORD_HPP
#define CSV_RECORD_HPP

#include <cstdint>
#include <string>
#include <variant>

/**
 * \brief Базовая структура для всех CSV записей
 * 
 * Содержит общие поля для level и trade файлов
 */
struct csv_record_base {
    std::uint64_t receive_ts;   ///< Временная метка получения (микросекунды)
    std::uint64_t exchange_ts;  ///< Временная метка биржи (микросекунды)
    double price;               ///< Цена
    double quantity;            ///< Объем
    std::string side;           ///< Сторона (bid/ask)
    
    /**
     * \brief Конструктор по умолчанию
     */
    csv_record_base() noexcept :
        receive_ts{0},
        exchange_ts{0},
        price{0.0},
        quantity{0.0},
        side{}
    {}
};

/**
 * \brief Запись из level.csv файла
 * 
 * Содержит дополнительное поле rebuild
 */
struct level_record : csv_record_base {
    int rebuild;  ///< Флаг перестроения стакана (0 или 1)
    
    /**
     * \brief Конструктор по умолчанию
     */
    level_record() noexcept :
        csv_record_base{},
        rebuild{0}
    {}
};

/**
 * \brief Запись из trade.csv файла
 * 
 * Использует только базовые поля
 */
struct trade_record : csv_record_base {
    /**
     * \brief Конструктор по умолчанию
     */
    trade_record() noexcept :
        csv_record_base{}
    {}
};

/**
 * \brief Вариант для хранения любого типа записи
 */
using csv_record = std::variant<level_record, trade_record>;

/**
 * \brief Получение receive_ts из любого типа записи
 * \param record_ запись CSV
 * \return временная метка receive_ts
 */
[[nodiscard]] inline std::uint64_t get_receive_ts(
    const csv_record& record_) noexcept {
    
    return std::visit([](const auto& rec) -> std::uint64_t {
        return rec.receive_ts;
    }, record_);
}

/**
 * \brief Получение price из любого типа записи
 * \param record_ запись CSV
 * \return цена
 */
[[nodiscard]] inline double get_price(const csv_record& record_) noexcept {
    return std::visit([](const auto& rec) -> double {
        return rec.price;
    }, record_);
}

#endif  // CSV_RECORD_HPP