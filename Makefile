SHELL := /bin/bash

BIN_DIR := bin
CLOUD_BIN := $(BIN_DIR)/meetagent-cloud
NODE_BIN := $(BIN_DIR)/meetagent-node
SIM_BIN := $(BIN_DIR)/meetagent-simulator
WEB_DIR := web/test-client

CLOUD_CONFIG ?= configs/cloud.yaml
NODE_CONFIG ?= configs/node.yaml
DOCKER ?= docker

.PHONY: all build cloud node simulator test fmt vet clean run-cloud run-node run-node-mock simulator-run dev-tls install-service docker-build docker-up docker-down docker-logs web-install web-dev web-build

all: build

## build: compile both Go binaries into ./bin
build: $(CLOUD_BIN) $(NODE_BIN)

$(CLOUD_BIN): $(shell find cmd internal pkg -name '*.go') go.mod go.sum
	@mkdir -p $(BIN_DIR)
	go build -o $(CLOUD_BIN) ./cmd/cloud

$(NODE_BIN): $(shell find cmd internal pkg -name '*.go') go.mod go.sum
	@mkdir -p $(BIN_DIR)
	go build -o $(NODE_BIN) ./cmd/node

cloud: $(CLOUD_BIN)
node: $(NODE_BIN)
simulator: $(SIM_BIN)

$(SIM_BIN): $(shell find cmd internal pkg -name '*.go') go.mod go.sum
	@mkdir -p $(BIN_DIR)
	go build -o $(SIM_BIN) ./cmd/simulator

## test: run the full Go test suite with the race detector
test:
	go test ./... -race -count=1

fmt:
	gofmt -w cmd internal pkg

vet:
	go vet ./...

clean:
	rm -rf $(BIN_DIR)
	rm -rf $(WEB_DIR)/dist

## run-cloud: start Cloud with the development configuration
run-cloud:
	go run ./cmd/cloud -config $(CLOUD_CONFIG)

## run-node: start the Node with the development configuration
run-node:
	go run ./cmd/node -config $(NODE_CONFIG)

## dev-tls: generate a self-signed certificate for local WSS testing
dev-tls:
	./scripts/dev-tls.sh

## install-service: install and start the Node as a systemd user service
install-service:
	./scripts/install-node-service.sh

## docker-build: build the Node container image
docker-build:
	$(DOCKER) compose build

## docker-up: start the Node container (stop meetagent-node.service first)
docker-up:
	$(DOCKER) compose up -d

## docker-down: stop and remove the Node container
docker-down:
	$(DOCKER) compose down

## docker-logs: follow Node container logs
docker-logs:
	$(DOCKER) compose logs -f node

## run-node-mock: start the Node with the mock Agent configuration
run-node-mock:
	go run ./cmd/node -config configs/node-mock.yaml

## simulator-run: connect the simulator to a local Node
simulator-run:
	go run ./cmd/simulator -url ws://127.0.0.1:8787/ws

web-install:
	cd $(WEB_DIR) && npm install

## web-dev: run the Vite dev server on http://127.0.0.1:5173
web-dev:
	cd $(WEB_DIR) && npm run dev

web-build:
	cd $(WEB_DIR) && npm run build