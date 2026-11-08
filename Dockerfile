# syntax=docker/dockerfile:1

FROM golang:1.24-alpine AS build
WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download
COPY cmd ./cmd
COPY internal ./internal
COPY pkg ./pkg
RUN CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" \
    -o /out/meetagent-node ./cmd/node

FROM alpine:3.22
RUN apk add --no-cache ca-certificates tzdata wget \
    && addgroup -g 1000 -S meetagent \
    && adduser -u 1000 -S -G meetagent -h /app meetagent
WORKDIR /app
COPY --from=build /out/meetagent-node /app/meetagent-node
COPY configs/node.yaml configs/node-mock.yaml ./configs/
RUN mkdir -p /app/data && chown -R meetagent:meetagent /app
USER meetagent

EXPOSE 8787
HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
  CMD wget -qO- http://127.0.0.1:8787/healthz >/dev/null || exit 1

ENTRYPOINT ["/app/meetagent-node"]
CMD ["-config", "configs/node.yaml"]
