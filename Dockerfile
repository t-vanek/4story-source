# syntax=docker/dockerfile:1
FROM ubuntu:24.04 AS build-deps
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build pkg-config \
        libboost-system-dev libssl-dev openssl libspdlog-dev \
        libtomlplusplus-dev libsoci-dev unixodbc-dev libpq-dev \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src

FROM build-deps AS build
COPY CMakeLists.txt ./
COPY Lib/3rdParty/bcrypt/ Lib/3rdParty/bcrypt/
COPY Lib/Own/TNetLib/ Lib/Own/TNetLib/
COPY Lib/Own/TProtocol/include/ Lib/Own/TProtocol/include/
COPY Lib/Own/FourStoryCommon/ Lib/Own/FourStoryCommon/
COPY Server/TLoginSvrAsio/ Server/TLoginSvrAsio/
COPY Server/TPatchSvrAsio/ Server/TPatchSvrAsio/
COPY Server/TLogSvrAsio/ Server/TLogSvrAsio/
COPY Server/TControlSvrAsio/ Server/TControlSvrAsio/
COPY Server/TMapSvrAsio/ Server/TMapSvrAsio/
COPY Server/TWorldSvrAsio/ Server/TWorldSvrAsio/
ARG BUILD_JOBS=2
RUN cmake -S . -B /build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
        -DFOURSTORY_BUILD_LEGACY_TNETLIB=OFF \
        -DCMAKE_INSTALL_PREFIX=/opt/fourstory \
    && cmake --build /build --parallel "${BUILD_JOBS}" \
    && cmake --install /build --strip

# Build this target separately to run the existing test suite with assertions.
FROM build AS test
ARG TEST_JOBS=2
RUN cmake -S . -B /tests -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
        -DFOURSTORY_BUILD_LEGACY_TNETLIB=OFF \
    && cmake --build /tests --parallel "${TEST_JOBS}" \
    && ctest --test-dir /tests --output-on-failure --timeout 60

FROM ubuntu:24.04 AS runtime
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl openssl libboost-system1.83.0 libssl3t64 \
        libspdlog1.12 libtomlplusplus3 libsoci-core4.0 \
        libsoci-odbc4.0 libsoci-sqlite3-4.0 libsoci-postgresql4.0 unixodbc odbcinst \
    && curl -fsSLo /tmp/microsoft-prod.deb \
        https://packages.microsoft.com/config/ubuntu/24.04/packages-microsoft-prod.deb \
    && dpkg -i /tmp/microsoft-prod.deb \
    && apt-get update \
    && ACCEPT_EULA=Y apt-get install -y --no-install-recommends msodbcsql18 \
    && rm -rf /var/lib/apt/lists/* /tmp/microsoft-prod.deb \
    && groupadd --gid 10001 fourstory \
    && useradd --uid 10001 --gid fourstory --create-home fourstory \
    && mkdir -p /etc/fourstory /var/lib/fourstory \
    && chown fourstory:fourstory /var/lib/fourstory
COPY --from=build /opt/fourstory/ /opt/fourstory/
ENV PATH="/opt/fourstory/bin:${PATH}"
WORKDIR /var/lib/fourstory
USER 10001:10001
# One daemon per container. Exec-form CMD forwards SIGTERM to the server.
CMD ["tloginsvr_asio", "--config", "/etc/fourstory/tloginsvr.toml"]
