/**
 * \file file_finder.hpp
 * \author github:PsiXich
 * \brief Поиск и фильтрация CSV файлов в директории
 * \date 2025-02-23
 * \version 2.0
 */

#ifndef FILE_FINDER_HPP
#define FILE_FINDER_HPP

#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace fs = std::filesystem;

/**
 * \brief Поиск и фильтрация файлов в директории
 */
class file_finder {
public:
    /**
     * \brief Поиск CSV файлов в директории с применением масок
     * \param directory_ директория для поиска
     * \param masks_ список масок для фильтрации имен файлов
     * \return вектор путей к найденным файлам
     * 
     * Если masks_ пуст, возвращаются все CSV файлы.
     * Маска проверяется на вхождение в имя файла (без учета регистра).
     */
    [[nodiscard]] static std::vector<fs::path> find_csv_files(
        const fs::path& directory_,
        const std::vector<std::string>& masks_) noexcept;

private:
    /**
     * \brief Проверка, является ли файл CSV
     * \param path_ путь к файлу
     * \return true если расширение .csv
     */
    [[nodiscard]] static bool is_csv_file(const fs::path& path_) noexcept;
    
    /**
     * \brief Проверка соответствия имени файла маскам
     * \param filename_ имя файла
     * \param masks_ список масок
     * \return true если имя содержит хотя бы одну маску
     * 
     * Если masks_ пуст, возвращается true.
     */
    [[nodiscard]] static bool matches_masks(
        const std::string& filename_,
        const std::vector<std::string>& masks_) noexcept;
    
    /**
     * \brief Преобразование строки в нижний регистр
     * \param str_ входная строка
     * \return строка в нижнем регистре
     */
    [[nodiscard]] static std::string to_lower(const std::string& str_) noexcept;
};

// ============================================================================
// Реализация
// ============================================================================

std::vector<fs::path> file_finder::find_csv_files(
    const fs::path& directory_,
    const std::vector<std::string>& masks_) noexcept {
    
    std::vector<fs::path> result;
    
    try {
        if (!fs::exists(directory_)) {
            spdlog::error("The directory does not exist: {}", 
                directory_.string());
            return result;
        }
        
        if (!fs::is_directory(directory_)) {
            spdlog::error("The path is not a directory: {}", 
                directory_.string());
            return result;
        }
        
        // Итерация по файлам в директории
        for (const auto& entry : fs::directory_iterator(directory_)) {
            if (!entry.is_regular_file()) {
                continue;
            }
            
            const auto& path = entry.path();
            
            // Проверка расширения .csv
            if (!is_csv_file(path)) {
                continue;
            }
            
            // Проверка соответствия маскам
            const auto filename = path.filename().string();
            if (!matches_masks(filename, masks_)) {
                continue;
            }
            
            result.push_back(path);
        }
        
        // Сортировка по имени файла для детерминированного порядка
        std::sort(result.begin(), result.end());
        
        spdlog::info("Files found: {}", result.size());
        for (const auto& file : result) {
            spdlog::info("  - {}", file.filename().string());
        }
        
    } catch (const fs::filesystem_error& err) {
        spdlog::error("Error scanning directory: {}", err.what());
    } catch (const std::exception& err) {
        spdlog::error("Unexpected error when searching for files: {}", err.what());
    }
    
    return result;
}

bool file_finder::is_csv_file(const fs::path& path_) noexcept {
    try {
        const auto extension = path_.extension().string();
        return to_lower(extension) == ".csv";
    } catch (...) {
        return false;
    }
}

bool file_finder::matches_masks(
    const std::string& filename_,
    const std::vector<std::string>& masks_) noexcept {
    
    try {
        // Если маски не указаны, файл подходит
        if (masks_.empty()) {
            return true;
        }
        
        const auto filename_lower = to_lower(filename_);
        
        // Проверка, содержит ли имя файла хотя бы одну маску
        for (const auto& mask : masks_) {
            const auto mask_lower = to_lower(mask);
            if (filename_lower.find(mask_lower) != std::string::npos) {
                return true;
            }
        }
        
        return false;
    } catch (...) {
        return false;
    }
}

std::string file_finder::to_lower(const std::string& str_) noexcept {
    try {
        std::string result = str_;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return result;
    } catch (...) {
        return str_;
    }
}

#endif  // FILE_FINDER_HPP