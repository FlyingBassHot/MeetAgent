SHELL := /bin/bash

BIN_DIR := bin
CLOUD_BIN := $(BIN_DIR)/meetagent-cloud
NODE_BIN := $(BIN_DIR)/meetagent-node
WEB_DIR := web/test-client

CLOUD_CONFIG ?= configs/cloud.yaml
NODE_CONFIG ?= configs/node.yaml

.PHONY: all build cloud node test fmt vet clean run-cloud run-node web-install web-dev web-build

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

web-install:
	cd $(WEB_DIR) && npm install

## web-dev: run the Vite dev server on http://127.0.0.1:5173
web-dev:
	cd $(WEB_DIR) && npm run dev

web-build:
	cd $(WEB_DIR) && npm run build