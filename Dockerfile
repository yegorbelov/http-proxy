FROM ubuntu:26.04 AS build

RUN apt-get update \
    && apt-get install -y build-essential cmake git

WORKDIR /src
COPY . .

RUN cmake -S . -B build -DENABLE_TESTS=ON \
    && cmake --build build -j$(nproc) \
    && cd build && ctest --output-on-failure

FROM ubuntu:26.04

WORKDIR /app
COPY --from=build /src/build/target_exec /app/target_exec
COPY config/ /app/config/

EXPOSE 18080
CMD ["/app/target_exec", "/app/config/default_config.ini"]