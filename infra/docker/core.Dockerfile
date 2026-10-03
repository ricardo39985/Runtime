FROM node:24-bookworm-slim AS web
WORKDIR /src
RUN npm install --global pnpm@10.11.0
COPY package.json pnpm-workspace.yaml pnpm-lock.yaml ./
COPY apps/web ./apps/web
COPY services/bridge/package.json ./services/bridge/package.json
COPY packages/contracts ./packages/contracts
RUN pnpm install --frozen-lockfile && pnpm --filter @runtime/web check && pnpm --filter @runtime/web build

FROM debian:trixie-slim AS cpp
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake ninja-build git curl ca-certificates zip unzip tar pkg-config autoconf automake libtool bison flex python3 && rm -rf /var/lib/apt/lists/*
WORKDIR /src
RUN git clone https://github.com/microsoft/vcpkg.git .tools/vcpkg && cd .tools/vcpkg && git checkout 3cbc1db4d867ec83c89fba4c461321c11f78b5e3 && ./bootstrap-vcpkg.sh -disableMetrics
COPY CMakeLists.txt CMakePresets.json vcpkg.json ./
COPY infra/triplets ./infra/triplets
COPY services/core ./services/core
COPY tests/unit ./tests/unit
RUN cmake -S . -B build/server -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/src/.tools/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-linux-runtime -DVCPKG_OVERLAY_TRIPLETS=/src/infra/triplets && cmake --build build/server --parallel 2 && ctest --test-dir build/server --output-on-failure

FROM debian:trixie-slim
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl libstdc++6 libgcc-s1 tini && rm -rf /var/lib/apt/lists/* && useradd --uid 10001 --create-home runtime
WORKDIR /app
COPY --from=cpp /src/build/server/runtime-core /app/runtime-core
COPY --from=web /src/apps/web/build /app/web
COPY packages/contracts /app/packages/contracts
ENV WEB_ROOT=/app/web CONTRACTS_DIR=/app/packages/contracts BIND_ADDRESS=0.0.0.0 PORT=8080
USER runtime
EXPOSE 8080
ENTRYPOINT ["/usr/bin/tini","--"]
CMD ["/app/runtime-core"]
