# CruiseGua Chat

一个实时群聊应用：后端是基于 Boost 的异步 C++20 服务，前端是 React / Next.js。

| 构建 | 文档 |
| --- | --- |
| ![Build Status](https://github.com/Fshying/CruiseGua-Chat/actions/workflows/build.yml/badge.svg) | [![Docs](https://github.com/Fshying/CruiseGua-Chat/actions/workflows/doc.yml/badge.svg)](https://fshying.github.io/CruiseGua-Chat/) |

## 项目简介

CruiseGua Chat 是一个网页版聊天应用。用户用邮箱和密码注册后，就可以在群聊（"聊天室"）里互相交流。消息通过 websocket 实时送达并持久化保存，重新连接后历史记录依然完整。

真正的看点在服务端：一个单线程、全异步的 C++ 服务，借助 **C++20 协程** 支撑大量并发连接，请求路径上没有任何阻塞调用。

## 功能特性

- 邮箱 + 密码注册与登录；密码使用 scrypt 哈希，会话基于签名 Cookie。
- 基于 websocket 的实时群聊，事件以 JSON 编码。
- 消息历史持久化，连接时加载，并支持按需分页拉取。
- 聊天室目前在服务端静态定义。
- 账号、认证等对延迟不敏感的操作走 REST API。
- C++ 服务同时以静态文件方式托管前端构建产物，因此一次部署只需要一个容器。
- 服务端与客户端均有单元测试，另有 Python 集成测试直接驱动真实的 HTTP 与 websocket 接口。

## 架构

| 层次 | 技术选型 |
| --- | --- |
| 服务端 | C++20、Boost.Asio、Boost.Beast、Boost.Redis、Boost.MySQL、Boost.JSON、OpenSSL |
| 前端 | React 19、Next.js、TypeScript、MUI、Tailwind CSS、react-hook-form |
| 消息存储 | Redis，开启持久化 |
| 账号存储 | MySQL |
| 通信方式 | 账号与认证走 REST，消息收发走 websocket |
| 测试 | Boost.Test（服务端）、Jest + Testing Library（客户端）、pytest（集成测试） |
| 构建与部署 | CMake、Docker、Docker Compose、GitHub Actions |

所有 I/O 都是异步的，由 Boost.Asio 驱动。协程（`co_spawn` 与 `boost::asio::awaitable<T>`）让业务逻辑保持易读，也让线程模型足够简单。Redis 存放低延迟数据（消息历史），MySQL 存放不需要亚毫秒级访问的数据（账号）。每种存储都封装在 `server/src/services/` 下的服务层之后，API 处理函数不会直接操作数据库客户端。

通信协议很小，且基于 JSON：客户端连接时，服务端推送 `hello` 事件（包含聊天室列表和最近的历史消息）；`clientMessages` 与 `serverMessages` 负责消息收发；`requestMessageHistory` 实现分页。

## 快速开始

想最快把整套服务跑起来，用 Docker Compose：一个 C++ 服务、一个 Redis 实例、一个 MySQL 实例，数据库表结构会自动初始化。

```bash
docker compose up --build
```

然后打开 http://localhost:8080 。不需要安装任何编译器或工具链。

## 本地开发

### 后端

需要 C++20 编译器、CMake，以及 OpenSSL 和 ICU 的开发文件：

```bash
sudo apt install g++ cmake libssl-dev libicu-dev
```

Boost 由 `tools/` 下的脚本安装：

```bash
./tools/install-boost.sh
```

用开发用的 compose 文件在默认端口启动 Redis 和 MySQL：

```bash
docker compose -f docker-compose-local.yml up
```

接着编译并运行服务端：

```bash
mkdir -p build && cd build
cmake -DCMAKE_PREFIX_PATH=$HOME/boost ../server
cmake --build . -j 4
./main 0.0.0.0 8080 .
```

### 前端

```bash
cd client
npm install
npm run dev
```

### 测试

```bash
# 服务端单元测试（在 build 目录下执行）
ctest --output-on-failure .

# 客户端单元测试
cd client && npm test

# 集成测试（需要服务端已在运行）
cd test/integration && pip install -r requirements.txt && pytest
```

## 仓库结构

| 路径 | 内容 |
| --- | --- |
| `server/` | C++ 服务端，按 `src/api`、`src/services`、`src/util` 分层 |
| `client/` | Next.js 前端（pages、components、hooks、lib） |
| `test/integration/` | 针对 REST 与 websocket 接口的 Python 集成测试 |
| `doc/` | AsciiDoc 文档，会发布到 GitHub Pages |
| `tools/` | Boost 安装脚本、部署脚本、文件头工具 |
| `Dockerfile` | 多阶段构建，产出可部署镜像 |

## 部署

`.github/workflows/build.yml` 会构建服务端与客户端、跑完整测试、把镜像推到 GitHub Container Registry，并通过 SSH 部署到目标主机。在仓库 secrets 中配置 `SSH_HOSTNAME`、`SSH_USERNAME`、`SSH_PRIVATE_KEY` 即可启用。运行时细节见 `docker-compose.yml` 与 `tools/deploy.sh`。

`.github/workflows/doc.yml` 使用 Asciidoctor 重新构建文档，并发布到 `gh-pages` 分支。

## 许可证

本项目基于 [Boost 软件许可证 1.0](LICENSE_1_0.txt) 分发，允许使用与修改代码。
