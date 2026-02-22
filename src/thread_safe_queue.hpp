/**
 * \file thread_safe_queue.hpp
 * \author github:PsiXich
 * \brief Потокобезопасная очередь для многопоточной обработки CSV данных
 * \date 2025-02-22
 * \version 2.0
 */

#ifndef THREAD_SAFE_QUEUE_HPP
#define THREAD_SAFE_QUEUE_HPP

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

/**
 * \brief Потокобезопасная очередь с блокировкой
 * \tparam T тип элементов очереди
 * 
 * Реализует паттерн Producer-Consumer.
 * Поддерживает множественных производителей и потребителей.
 */
template<typename T>
class thread_safe_queue {
public:
    /**
     * \brief Конструктор по умолчанию
     */
    thread_safe_queue() noexcept : _finished{false} {}
    
    /**
     * \brief Запретить копирование
     */
    thread_safe_queue(const thread_safe_queue&) = delete;
    thread_safe_queue& operator=(const thread_safe_queue&) = delete;
    
    /**
     * \brief Добавить элемент в очередь (move семантика)
     * \param item_ элемент для добавления
     */
    void push(T item_) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _queue.push(std::move(item_));
        }
        _condition.notify_one();
    }
    
    /**
     * \brief Попытка извлечь элемент из очереди (неблокирующая)
     * \return элемент или nullopt если очередь пуста
     */
    [[nodiscard]] std::optional<T> try_pop() noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (_queue.empty()) {
            return std::nullopt;
        }
        
        T item = std::move(_queue.front());
        _queue.pop();
        return item;
    }
    
    /**
     * \brief Извлечь элемент из очереди (блокирующая операция)
     * \return элемент или nullopt если очередь завершена
     * 
     * Блокирует поток до появления элемента или завершения очереди.
     */
    [[nodiscard]] std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(_mutex);
        
        // Ждём пока не появится элемент или очередь не завершится
        _condition.wait(lock, [this] {
            return !_queue.empty() || _finished;
        });
        
        if (_queue.empty()) {
            return std::nullopt;  // Очередь завершена
        }
        
        T item = std::move(_queue.front());
        _queue.pop();
        return item;
    }
    
    /**
     * \brief Извлечь элемент с таймаутом
     * \param timeout_ максимальное время ожидания
     * \return элемент или nullopt если таймаут или очередь пуста
     */
    template<typename Rep, typename Period>
    [[nodiscard]] std::optional<T> pop_for(
        const std::chrono::duration<Rep, Period>& timeout_) {
        
        std::unique_lock<std::mutex> lock(_mutex);
        
        if (!_condition.wait_for(lock, timeout_, [this] {
            return !_queue.empty() || _finished;
        })) {
            return std::nullopt;  // Таймаут
        }
        
        if (_queue.empty()) {
            return std::nullopt;  // Очередь завершена
        }
        
        T item = std::move(_queue.front());
        _queue.pop();
        return item;
    }
    
    /**
     * \brief Проверить пуста ли очередь
     * \return true если очередь пуста
     */
    [[nodiscard]] bool empty() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.empty();
    }
    
    /**
     * \brief Получить размер очереди
     * \return количество элементов в очереди
     */
    [[nodiscard]] std::size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.size();
    }
    
    /**
     * \brief Сигнализировать о завершении добавления элементов
     * 
     * После вызова этого метода все блокированные потоки
     * в pop() будут разблокированы и получат nullopt.
     */
    void finish() noexcept {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _finished = true;
        }
        _condition.notify_all();
    }
    
    /**
     * \brief Проверить завершена ли очередь
     * \return true если очередь завершена
     */
    [[nodiscard]] bool is_finished() const noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        return _finished;
    }
    
    /**
     * \brief Очистить очередь
     */
    void clear() noexcept {
        std::lock_guard<std::mutex> lock(_mutex);
        std::queue<T> empty_queue;
        std::swap(_queue, empty_queue);
    }

private:
    mutable std::mutex _mutex;              ///< Мьютекс для синхронизации
    std::condition_variable _condition;     ///< Условная переменная для ожидания
    std::queue<T> _queue;                   ///< Внутренняя очередь
    bool _finished;                         ///< Флаг завершения
};

#endif  // THREAD_SAFE_QUEUE_HPP