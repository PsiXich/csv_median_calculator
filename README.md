# CSV Median Calculator

**Версия:** 2.1.0  
**Язык:** C++23  
**Лицензия:** MIT

Высокопроизводительное приложение для инкрементального расчета медианы цен из CSV-файлов биржевых торгов с поддержкой многопоточной и потоковой обработки.

---

## 🚀 Быстрый старт

### Сборка

```bash
# Клонировать репозиторий
git clone https://github.com/your-username/csv_median_calculator.git
cd csv_median_calculator

# Создать директорию сборки
mkdir build && cd build

# Настроить и собрать
cmake ..
cmake --build . -j$(nproc)
```

### Запуск

```bash
# Сгенерировать тестовые данные
./generate_test_data 1000 500

# Запустить приложение
./csv_median_calculator --config ../config.toml
```

**Результат:** файл `examples/output/median_result.csv` с рассчитанными медианами.

---

## 📋 Требования

### Компилятор и инструменты

- **CMake** 3.23+
- **C++23 компилятор:**
  - GCC 13+ (Linux)
  - Clang 16+ (macOS)  
  - MSVC 19.35+ или MinGW-w64 (Windows)

### Зависимости

Скачиваются автоматически через CMake FetchContent:

- **Boost 1.84.0** — Program_options, Accumulators
- **toml++ 3.4.0** — парсинг конфигурации
- **spdlog 1.13.0** — логирование

---

## 🔨 Установка

<details>
<summary><b>Windows (MinGW/MSYS2)</b></summary>

```bash
# Открыть MSYS2 MinGW64 терминал
cd csv_median_calculator

mkdir build && cd build
cmake .. -DENABLE_STRICT_WARNINGS=OFF
cmake --build . -j$(nproc)
```
</details>

<details>
<summary><b>Linux (Ubuntu/Debian)</b></summary>

```bash
# Установить зависимости
sudo apt-get update
sudo apt-get install build-essential cmake git

# Собрать
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
```
</details>

<details>
<summary><b>macOS</b></summary>

```bash
# Установить инструменты
xcode-select --install
brew install cmake

# Собрать
mkdir build && cd build
cmake ..
cmake --build . -j$(sysctl -n hw.ncpu)
```
</details>

### Проверка установки

```bash
cd build
./csv_median_calculator --help
```

---

## 📖 Использование

### Режимы работы

Приложение поддерживает **3 режима обработки**:

| Режим | Команда | Применение |
|-------|---------|------------|
| **Multi-threaded** | `./csv_median_calculator` | По умолчанию, файлы <RAM |
| **Single-threaded** | `./csv_median_calculator -s` | Отладка, базовая обработка |
| **Streaming** | `./csv_median_calculator -m` | Файлы >RAM, экономия памяти |
| **Benchmark** | `./csv_median_calculator -b` | Сравнение всех режимов |

### Примеры использования

```bash
# Многопоточный режим (v2.0) - по умолчанию
./csv_median_calculator -config config.toml

# Однопоточный режим (v1.0)
./csv_median_calculator -s

# Потоковый режим (v2.1) - для больших файлов
./csv_median_calculator -m --batch-size 10000

# Benchmark - сравнение производительности
./csv_median_calculator -b

# Справка
./csv_median_calculator --help
```

### Генерация тестовых данных

```bash
# Маленький датасет (1.5k записей)
./generate_test_data 1000 500

# Средний датасет (150k записей)
./generate_test_data 100000 50000

# Большой датасет (1.5M записей)
./generate_test_data 1000000 500000
```

Параметры: `<level_records> <trade_records>`

Файлы создаются в `examples/input/`.

---

## ✨ Функциональность

### Основные возможности

- ✅ **Инкрементальный расчет медианы** — P² алгоритм (Boost.Accumulators)
- ✅ **Многопоточная обработка** — параллельное чтение файлов
- ✅ **Потоковая обработка** — файлы больше RAM
- ✅ **Автоматическая оптимизация** — запись только изменений медианы
- ✅ **Фильтрация файлов** — по маскам имен
- ✅ **Встроенный benchmark** — сравнение режимов

### Производительность

**Тестовый датасет:** 1500 записей, 2 файла

| Режим | Время | Speedup | Memory |
|-------|-------|---------|--------|
| Single-threaded (v1.0) | 156 ms | 1.0x | 2 MB |
| Multi-threaded (v2.0) | 134 ms | **1.16x** | 3 MB |
| Streaming (v2.1) | 145 ms | 1.08x | **<1 MB** |

**Большие файлы (100k+ записей):**
- Multi-threaded: **1.5-2.5x** ускорение
- Streaming: работает на файлах **любого размера**

### Версии

#### v1.0 — Базовая функциональность
- Последовательная обработка CSV файлов
- Инкрементальный расчет медианы
- TOML конфигурация

#### v2.0 — Многопоточность
- Параллельное чтение файлов
- Потокобезопасная очередь
- Ускорение 1.2-2.5x на многоядерных CPU

#### v2.1 — Потоковая обработка
- Батч-обработка (configurable batch size)
- Обработка файлов >RAM
- Memory-efficient режим

---

## ⚙️ Конфигурация

### Файл config.toml

```toml
[main]
    # Директория с входными CSV файлами (обязательно)
    input = './examples/input'
    
    # Директория для результатов (опционально, default: ./output)
    output = './examples/output'
    
    # Маски для фильтрации файлов (опционально, пусто = все файлы)
    filename_mask = ['level', 'trade']
```

### Параметры

| Параметр | Тип | Обязательный | Описание |
|----------|-----|--------------|----------|
| `input` | string | Да | Путь к входным CSV |
| `output` | string | Нет | Путь для результатов |
| `filename_mask` | array | Нет | Фильтр файлов |

---

## 📊 Формат данных

### Входные CSV файлы

**level.csv** — состояние стакана заявок
```csv
receive_ts;exchange_ts;price;quantity;side;rebuild
1716810808593627;1716810808574000;68480.00;10.109;bid;1
```

**trade.csv** — совершенные сделки
```csv
receive_ts;exchange_ts;price;quantity;side
1716810808663260;1716810808661000;68480.10;0.011;bid
```

### Выходной файл

**median_result.csv** — медианы цен
```csv
receive_ts;price_median
1716810808663260;68480.10000000
1716810809314641;68480.05000000
```

**Важно:** записываются только **изменения медианы**.

---

## 🏗️ Архитектура

### Структура проекта

```
csv_median_calculator/
├── CMakeLists.txt              # Конфигурация сборки
├── README.md                   # Документация
├── config.toml                 # Конфигурация
│
├── src/                        # Исходный код (header-only)
│   ├── main.cpp                # Точка входа
│   ├── config_parser.hpp       # Парсинг TOML
│   ├── csv_reader.hpp          # Базовый CSV reader
│   ├── csv_record.hpp          # Структуры данных
│   ├── median_calculator.hpp   # P² алгоритм медианы
│   │
│   ├── thread_safe_queue.hpp   # v2.0: Потокобезопасная очередь
│   ├── parallel_csv_reader.hpp # v2.0: Параллельное чтение
│   ├── data_processor.hpp      # v2.0: Consumer
│   │
│   ├── streaming_csv_reader.hpp      # v2.1: Streaming reader
│   ├── streaming_parallel_reader.hpp # v2.1: Stream + Parallel
│
├── examples/
│   ├── input/                  # Входные CSV
│   └── output/                 # Результаты
│
└── build/                      # Артефакты сборки
```

### Многопоточная архитектура (v2.0)

```
Reader Thread #1 ──┐
Reader Thread #2 ──┼──→ Thread-Safe Queue ──→ Processor ──→ Results
Reader Thread #N ──┘
```

**Producer-Consumer pattern:**
- **Producers:** читают файлы параллельно
- **Queue:** потокобезопасная передача данных
- **Consumer:** сортировка и расчет медианы

### Потоковая обработка (v2.1)

```
File 1GB:
├── Batch 1 (10k records) → Process → Free memory
├── Batch 2 (10k records) → Process → Free memory
├── Batch 3 (10k records) → Process → Free memory
└── ...

Peak Memory: ~размер 1 батча × кол-во файлов
```

---

## 💻 Технологии

### C++23

- `std::filesystem` — кроссплатформенная работа с файлами
- `std::optional` — безопасная обработка отсутствующих значений
- `std::variant` — type-safe хранение разных типов
- `std::thread`, `std::mutex`, `std::condition_variable` — многопоточность
- `std::atomic` — lock-free счетчики

### Библиотеки

**Boost.Accumulators** — P² алгоритм медианы
- O(1) память
- O(1) время на добавление значения
- Не требует хранения всех данных

**Boost.Program_options** — парсинг CLI аргументов

**toml++** — header-only TOML парсер

**spdlog** — высокопроизводительное логирование

### Паттерны

- **Producer-Consumer** — многопоточная обработка
- **Streaming** — батч-обработка больших файлов
- **Header-only** — упрощенная интеграция
- **RAII** — автоматическое управление ресурсами

---

## 🔧 Разработка

### Git Workflow

- `main` — стабильные релизы
- `dev` — активная разработка

### Code Style

**snake_case** для всех идентификаторов:

```cpp
class csv_reader;                  // классы
void calculate_median();           // функции
int _member_variable;              // члены класса (префикс _)
void process_data(int value_);    // параметры (суффикс _)
```

---

## 📈 Планы развития

### v2.2 (в разработке)
- Пул потоков (thread pool)
- Параллельная сортировка
- Unit тесты (Google Test)

### v3.0 (идеи)
- REST API
- Дополнительные метрики (среднее, квантили)
- Docker контейнер
- Real-time обработка

---

## 🙏 Благодарности

Проект создан в рамках тестового задания для позиции C++ разработчика.

**Используемые библиотеки:**
- [Boost](https://www.boost.org/)
- [toml++](https://github.com/marzer/tomlplusplus)
- [spdlog](https://github.com/gabime/spdlog)

---

**Автор:** PsiXich  
**Версия:** 2.1
**Обновлено:** 23 февраля 2025