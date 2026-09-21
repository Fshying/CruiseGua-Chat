# CruiseGua Chat

A real-time group chat application, with an asynchronous C++20 backend built on
Boost and a React / Next.js frontend.

| Build | Docs |
| --- | --- |
| ![Build Status](https://github.com/Fshying/CruiseGua-Chat/actions/workflows/build.yml/badge.svg) | [![Docs](https://github.com/Fshying/CruiseGua-Chat/actions/workflows/doc.yml/badge.svg)](https://fshying.github.io/CruiseGua-Chat/) |

## Overview

CruiseGua Chat is a web chat app where users register with an email and a
password, then talk to each other in group chats ("chat rooms"). Messages are
delivered in real time over websockets and are persisted, so history is still
there when you reconnect.

The interesting part is the backend: a single-threaded, fully asynchronous C++
server that handles many concurrent connections using **C++20 coroutines**, with
no blocking calls in the request path.

## Features

- Account creation and login with email + password; passwords are hashed with
  scrypt and sessions use signed cookies.
- Real-time group messaging over websockets, with JSON-encoded events.
- Persistent message history, loaded on connect and paginated on demand.
- Chat rooms are currently defined statically on the server.
- REST API for everything that does not need low latency (accounts, auth).
- The C++ server serves the compiled frontend as static files, so a deployment
  is a single container.
- Unit tests for both server and client, plus Python integration tests driving
  the real HTTP and websocket APIs.

## Architecture

| Layer | Technology |
| --- | --- |
| Server | C++20, Boost.Asio, Boost.Beast, Boost.Redis, Boost.MySQL, Boost.JSON, OpenSSL |
| Frontend | React 19, Next.js, TypeScript, MUI, Tailwind CSS, react-hook-form |
| Message store | Redis, running with persistence enabled |
| Account store | MySQL |
| Transports | REST for accounts and auth, websockets for messaging |
| Tests | Boost.Test (server), Jest + Testing Library (client), pytest (integration) |
| Build and deploy | CMake, Docker, Docker Compose, GitHub Actions |

All I/O is asynchronous and driven by Boost.Asio. Coroutines (`co_spawn` and
`boost::asio::awaitable<T>`) keep the logic readable and the threading model
trivial. Redis holds the low-latency data (message history), while MySQL holds
data that does not need sub-millisecond access (accounts). Each store sits
behind a small service layer in `server/src/services/`, so API handlers never
talk to a database client directly.

The protocol is small and JSON-based: the server pushes a `hello` event with the
room list and recent history when a client connects, `clientMessages` and
`serverMessages` carry chat messages, and `requestMessageHistory` implements
pagination.

## Quick start

Docker Compose is the fastest way to get everything running: a C++ server, a
Redis instance and a MySQL instance, with the database schema applied
automatically.

```bash
docker compose up --build
```

Then open http://localhost:8080. No compiler or toolchain needed.

## Local development

### Backend

You need a C++20 compiler, CMake, and the OpenSSL and ICU development files:

```bash
sudo apt install g++ cmake libssl-dev libicu-dev
```

Boost is installed by the helper script in `tools/`:

```bash
./tools/install-boost.sh
```

Start Redis and MySQL on their default ports with the development compose file:

```bash
docker compose -f docker-compose-local.yml up
```

Then build and run the server:

```bash
mkdir -p build && cd build
cmake -DCMAKE_PREFIX_PATH=$HOME/boost ../server
cmake --build . -j 4
./main 0.0.0.0 8080 .
```

### Frontend

```bash
cd client
npm install
npm run dev
```

### Tests

```bash
# Server unit tests, from the build directory
ctest --output-on-failure .

# Client unit tests
cd client && npm test

# Integration tests, against a running server
cd test/integration && pip install -r requirements.txt && pytest
```

## Repository layout

| Path | Contents |
| --- | --- |
| `server/` | C++ server, organized into `src/api`, `src/services` and `src/util` |
| `client/` | Next.js frontend (pages, components, hooks, lib) |
| `test/integration/` | Python integration tests for the REST and websocket APIs |
| `doc/` | AsciiDoc documentation, published to GitHub Pages |
| `tools/` | Boost installer, deployment script, file header helper |
| `Dockerfile` | Multi-stage build producing the deployable image |

## Deployment

`.github/workflows/build.yml` builds the server and the client, runs the test
suite, pushes the resulting image to the GitHub Container Registry and deploys
it to a target host over SSH. Setting `SSH_HOSTNAME`, `SSH_USERNAME` and
`SSH_PRIVATE_KEY` in the repository secrets is enough to enable it. See
`docker-compose.yml` and `tools/deploy.sh` for the runtime details.

`.github/workflows/doc.yml` rebuilds the documentation with Asciidoctor and
publishes it to the `gh-pages` branch.

## Credits

This project is a derivative of
[BoostServerTech Chat](https://github.com/anarthal/servertech-chat) by
Ruben Perez ([@anarthal](https://github.com/anarthal)), part of the
[BoostServerTech](https://docs.google.com/document/d/1ZQrod1crs8EaNLLqSYIRMacwR3Rv0hC5l-gfL-jOp2M)
series. It is distributed under the
[Boost Software License 1.0](LICENSE_1_0.txt), which permits use and
modification of the code as long as the license and copyright notices are
preserved.
