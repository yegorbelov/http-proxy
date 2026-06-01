# HTTP-прокси

Сервер-прокси: фильтр URL, кеш ответов (LRU, лимиты), журнал, конфиг. Реализация на C++17.

## Сторонние библиотеки

- **pthread** (через `Threads` в CMake, для `std::thread` на некоторых системах) и **стандартная библиотека** C++ / POSIX (`socket`, `getaddrinfo`).

## Сборка

```sh
cmake -S . -B build
cmake --build build
```

## Запуск

```sh
cd build
./target_exec ../config/default_config.ini
```

По умолчанию, если путь к конфигу не указан, используется `config/default_config.ini` (от рабочей директории). В конфиге по умолчанию порт **18080**.

Пример: настроить в системе/браузере HTTP-прокси `127.0.0.1:18080` и открыть, например, `http://neverssl.com` (только `http:`, `https` / `CONNECT` в этой проекте не реализованы).

Или через curl:

```sh
curl -v -x http://127.0.0.1:18080 http://neverssl.com
```

## Тестирование

Сборка с GoogleTest и мини-сценариями включается по умолчанию (`ENABLE_TESTS=ON`).

```sh
cmake -S . -B build -DENABLE_TESTS=ON
cmake --build build
cd build && ctest --output-on-failure
```

Запуск только unit-тестов.

```sh
./build/tests/proxy_unit_tests
```

Примеры (`-` в **первом** аргументе — пустой список allow, затем deny и URL):

```sh
printf '%s\n' 'GET http://example.com/foo HTTP/1.1' 'Host: example.com' '' |
  ./build/tests/scenario_parse_request
./build/tests/scenario_url_policy - 'bad.com' 'http://bad.com/x'
```

Запуск конкретной группы тестов

```sh
ctest --output-on-failure -R EventLogEvictionTest*
```

## Docker

Тесты — **GoogleTest**, в контейнере запускаются через `ctest` при сборке образа.

```sh
docker build -t proxy .
docker run --rm -p 18080:18080 proxy
```
