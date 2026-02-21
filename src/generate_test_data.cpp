/**
 * \file generate_test_data.cpp
 * \author github:PsiXich
 * \brief Генератор тестовых CSV файлов для биржевых данных
 * \date 2025-02-21
 * \version 1.4
 */

#include <iostream>
#include <fstream>
#include <random>
#include <iomanip>
#include <cstdint>
#include <filesystem>

namespace fs = std::filesystem;

/**
 * \brief Генерация level.csv файла
 * \param filepath_ полный путь к файлу
 * \param num_records_ количество записей
 */
void generate_level_csv(const fs::path& filepath_, std::size_t num_records_) {
    std::ofstream file(filepath_);
    if (!file.is_open()) {
        std::cerr << "Failed to create file: " << filepath_ << "\n";
        return;
    }
    
    // Заголовок
    file << "receive_ts;exchange_ts;price;quantity;side;rebuild\n";
    
    // Настройка генератора случайных чисел
    std::random_device rd;
    std::mt19937_64 gen(rd());
    
    std::uniform_real_distribution<double> price_dist(68400.0, 68500.0);
    std::uniform_real_distribution<double> quantity_dist(0.001, 15.0);
    std::uniform_int_distribution<int> rebuild_dist(0, 1);
    std::uniform_int_distribution<int> side_dist(0, 1);
    
    // Начальная временная метка (microseconds)
    std::uint64_t base_ts = 1716810808000000ULL;
    
    // Генерация данных
    file << std::fixed << std::setprecision(8);
    
    for (std::size_t i = 0; i < num_records_; ++i) {
        std::uint64_t receive_ts = base_ts + i * 1000 + (gen() % 1000);
        std::uint64_t exchange_ts = receive_ts - (gen() % 50000);
        
        double price = price_dist(gen);
        double quantity = quantity_dist(gen);
        const char* side = side_dist(gen) ? "bid" : "ask";
        int rebuild = (i % 100 == 0) ? 1 : rebuild_dist(gen);
        
        file << receive_ts << ";"
             << exchange_ts << ";"
             << price << ";"
             << quantity << ";"
             << side << ";"
             << rebuild << "\n";
    }
    
    file.close();
    std::cout << "Generated " << filepath_.filename().string()
              << " (" << num_records_ << " records)\n";
}

/**
 * \brief Генерация trade.csv файла
 * \param filepath_ полный путь к файлу
 * \param num_records_ количество записей
 */
void generate_trade_csv(const fs::path& filepath_, std::size_t num_records_) {
    std::ofstream file(filepath_);
    if (!file.is_open()) {
        std::cerr << "Failed to create file: " << filepath_ << "\n";
        return;
    }
    
    // Заголовок
    file << "receive_ts;exchange_ts;price;quantity;side\n";
    
    // Настройка генератора случайных чисел
    std::random_device rd;
    std::mt19937_64 gen(rd());
    
    std::uniform_real_distribution<double> price_dist(68400.0, 68500.0);
    std::uniform_real_distribution<double> quantity_dist(0.001, 5.0);
    std::uniform_int_distribution<int> side_dist(0, 1);
    
    // Начальная временная метка (microseconds), смещенная относительно level
    std::uint64_t base_ts = 1716810808500000ULL;
    
    // Генерация данных
    file << std::fixed << std::setprecision(8);
    
    for (std::size_t i = 0; i < num_records_; ++i) {
        std::uint64_t receive_ts = base_ts + i * 500 + (gen() % 500);
        std::uint64_t exchange_ts = receive_ts - (gen() % 30000);
        
        double price = price_dist(gen);
        double quantity = quantity_dist(gen);
        const char* side = side_dist(gen) ? "bid" : "ask";
        
        file << receive_ts << ";"
             << exchange_ts << ";"
             << price << ";"
             << quantity << ";"
             << side << "\n";
    }
    
    file.close();
    std::cout << "Generated " << filepath_.filename().string()
              << " (" << num_records_ << " records)\n";
}

/**
 * \brief Определение целевой директории для генерации файлов
 * \return путь к директории examples/input
 */
fs::path get_output_directory() {
    // Попытка найти директорию examples/input относительно разных путей
    fs::path current = fs::current_path();
    
    // Вариант 1: ../examples/input (если запускаем из build/)
    fs::path path1 = current / ".." / "examples" / "input";
    if (fs::exists(path1.parent_path())) {
        if (!fs::exists(path1)) {
            fs::create_directories(path1);
            std::cout << "Created directory: " << path1 << "\n";
        }
        return fs::canonical(path1);
    }
    
    // Вариант 2: ./examples/input (если запускаем из корня проекта)
    fs::path path2 = current / "examples" / "input";
    if (fs::exists(path2.parent_path())) {
        if (!fs::exists(path2)) {
            fs::create_directories(path2);
            std::cout << "Created directory: " << path2 << "\n";
        }
        return fs::canonical(path2);
    }
    
    // Вариант 3: создаём в текущей директории
    fs::path path3 = current / "examples" / "input";
    fs::create_directories(path3);
    std::cout << "Created directory: " << path3 << "\n";
    return path3;
}

int main(int argc, char* argv[]) {
    std::size_t num_level_records = 1000;
    std::size_t num_trade_records = 500;
    
    // Парсинг аргументов командной строки
    if (argc > 1) {
        try {
            num_level_records = std::stoull(argv[1]);
        } catch (...) {
            std::cerr << "Invalid number for level records\n";
            return 1;
        }
    }
    if (argc > 2) {
        try {
            num_trade_records = std::stoull(argv[2]);
        } catch (...) {
            std::cerr << "Invalid number for trade records\n";
            return 1;
        }
    }
    
    std::cout << "========================================\n";
    std::cout << "  Test Data Generator\n";
    std::cout << "========================================\n";
    std::cout << "Level records: " << num_level_records << "\n";
    std::cout << "Trade records: " << num_trade_records << "\n";
    std::cout << "\n";
    
    try {
        // Определяем целевую директорию
        fs::path output_dir = get_output_directory();
        std::cout << "Output directory: " << output_dir << "\n\n";
        
        // Генерируем файлы сразу в нужной директории
        fs::path level_file = output_dir / "btcusdt_level_2024.csv";
        fs::path trade_file = output_dir / "btcusdt_trade_2024.csv";
        
        generate_level_csv(level_file, num_level_records);
        generate_trade_csv(trade_file, num_trade_records);
        
        std::cout << "\n========================================\n";
        std::cout << "Completed successfully!\n";
        std::cout << "Files are ready in: " << output_dir << "\n";
        std::cout << "========================================\n";
        
    } catch (const fs::filesystem_error& err) {
        std::cerr << "Filesystem error: " << err.what() << "\n";
        return 1;
    } catch (const std::exception& err) {
        std::cerr << "Error: " << err.what() << "\n";
        return 1;
    }
    
    return 0;
}