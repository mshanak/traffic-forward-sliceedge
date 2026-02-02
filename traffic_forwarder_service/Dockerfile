# --- Build stage (Alpine / musl) ---
FROM alpine:3.20 AS build

# Toolchain
RUN apk add --no-cache build-base cmake

WORKDIR /app
COPY CMakeLists.txt .
COPY src ./src

# Build (Release)
RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j

# --- Runtime stage (Alpine, lightweight) ---
FROM alpine:3.20

# C++ runtime (libstdc++)
RUN apk add --no-cache libstdc++

WORKDIR /app

# Copy binary
COPY --from=build /app/build/port_service_forwarder .

# Add entrypoint to read env and build args
COPY entrypoint.sh /app/entrypoint.sh
RUN chmod +x /app/entrypoint.sh

# Non-root user (optional)
RUN adduser -D appuser
USER appuser

# Default envs (can be overridden at run-time)
ENV BIND_IP=0.0.0.0
# Space-separated list like: "tcp:8080=1.2.3.4:80 udp:5000=5.6.7.8:5000"
ENV MAPPINGS="tcp:8080=127.0.0.1:80"

ENTRYPOINT ["/app/entrypoint.sh"]
# Leave CMD empty so env-based defaults are used; passing args to `docker run` overrides.
CMD []
