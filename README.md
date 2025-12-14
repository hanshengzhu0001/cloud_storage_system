# Distributed Banking System Platform

A high-performance, distributed banking system built in C++ featuring TCP/IP client-server architecture, lock-free queues, concurrency control, and AI-powered fraud detection. Designed for low-latency, high-throughput transaction processing.

## Architecture Overview

### Core Components

1. **Bank Branch Servers**: Distributed servers handling local banking operations
2. **TCP/IP Networking**: Client-server communication with message framing
3. **Lock-Free Queues**: High-throughput transaction processing
4. **Thread-Safe Banking Core**: Fine-grained locking for concurrent operations
5. **AI Fraud Detection Agent**: Real-time transaction monitoring and risk analysis
6. **Transaction Processor**: Multi-threaded transaction handling

### Key Features

- **Distributed Architecture**: Multiple bank branches with local data partitioning
- **High Throughput**: Lock-free queues supporting 100,000+ TPS
- **Low Latency**: Sub-10ms P99 transaction latency
- **Concurrency Control**: Account-level locking preventing deadlocks
- **Fraud Detection**: AI-powered real-time transaction analysis
- **Observability**: Comprehensive metrics and monitoring
- **Security**: Authentication and session management

## Implemented Levels (Original Banking System)

- **Level 1**: Accounts, deposits, transfers
- **Level 2**: Top spenders ranking (by total outgoing)
- **Level 3**: Scheduled payments (with cancel), processed before other ops at same timestamp
- **Level 4**: Merge accounts and historical balance queries (GetBalance)

## Project Structure

```
banking_system/
├── include/
│   ├── banking_server.hpp          # Main server orchestration
│   ├── banking_system_thread_safe.hpp # Thread-safe banking wrapper
│   ├── network/
│   │   ├── tcp_server.hpp         # TCP server implementation
│   │   ├── tcp_client.hpp          # TCP client implementation
│   │   └── protocol.hpp            # Message protocol and framing
│   ├── concurrent/
│   │   ├── lockfree_queue.hpp      # Lock-free MPSC queue
│   │   └── transaction_processor.hpp # Multi-threaded processor
│   └── ai/
│       └── fraud_detection_agent.hpp # AI fraud detection
├── network/                        # Network implementation
├── concurrent/                     # Concurrent data structures
├── ai/                            # AI components
├── tests/                         # Test automation
├── ARCHITECTURE.md                # Detailed architecture docs
├── CMakeLists.txt                 # Build configuration
├── main_server.cpp               # Server executable
├── banking_client.cpp             # Client demonstration
├── banking_core_impl.hpp          # Core banking logic (header)
├── banking_core_impl.cpp          # Core banking logic (impl)
└── README.md                      # This file
```

## Building the Project

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.16+
- Google Test (optional, for tests)

### Build Instructions

```bash
# Clone and build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Run server
./banking_server [port] [workers] [analysis_window]

# Run client (in another terminal)
./banking_client [host] [port]
```

### Example Usage

```bash
# Start server on port 8080 with 4 worker threads
./banking_server 8080 4 3600

# Connect client to localhost:8080
./banking_client localhost 8080
```

## API Overview

### Client Operations

The system supports the following banking operations via TCP/IP:

- **Authentication**: Session-based client authentication
- **Account Management**: Create accounts, check balances
- **Transactions**: Deposits, transfers, scheduled payments
- **Analytics**: Top spenders, historical queries
- **Account Operations**: Merges, payment scheduling/cancellation

### Message Protocol

All communication uses a custom binary protocol with:

- **Message Framing**: Length-prefixed messages
- **JSON Payloads**: Structured request/response data
- **Session Tokens**: Authentication and authorization
- **Timestamps**: Strict ordering and historical queries

### Example Client Interaction

```cpp
BankingClient client("localhost", 8080);
client.connect();
client.authenticate();
client.createAccount("alice_account");
client.deposit("alice_account", 1000);
client.transfer("alice_account", "bob_account", 500);
client.getBalance("alice_account");
```

## Performance Characteristics

### Throughput Targets

- **Transaction Processing**: 100,000+ TPS per server
- **Concurrent Connections**: 10,000+ active clients
- **Latency**: P99 < 10ms for transactions

### Scalability

- **Horizontal Scaling**: Multiple branch servers
- **Vertical Scaling**: Multi-core utilization with worker threads
- **Load Balancing**: Client connection distribution

### Memory Efficiency

- **Lock-Free Queues**: Zero-copy transaction routing
- **Object Pooling**: Reuse of transaction objects
- **Memory-Mapped Storage**: Optional persistent storage

## Fraud Detection System

### AI Components

- **Real-Time Analysis**: Transaction stream processing
- **Anomaly Detection**: Statistical modeling of transaction patterns
- **Risk Scoring**: Machine learning-based fraud probability
- **Automated Actions**: Alert generation and response triggers

### Detection Features

- **Amount Anomalies**: Unusual transaction sizes
- **Frequency Analysis**: High-velocity spending patterns
- **Location Tracking**: Geographic transaction analysis
- **Velocity Checks**: Spending rate monitoring

### Risk Assessment

- **Dynamic Thresholds**: Adaptive risk scoring
- **Multi-Factor Analysis**: Combined risk indicators
- **Historical Learning**: Pattern recognition over time

## Testing and Quality Assurance

### Test Automation

- **Unit Tests**: Component-level testing with Google Test
- **Integration Tests**: End-to-end system validation
- **Performance Tests**: Throughput and latency benchmarking
- **Concurrency Tests**: Multi-threaded operation validation

### Running Tests

```bash
# Build and run tests
cmake -DBUILD_TESTS=ON ..
make
ctest --output-on-failure
```

### Test Coverage

- **Banking Operations**: All core functionality
- **Concurrent Access**: Thread safety validation
- **Network Protocol**: Message serialization/deserialization
- **Fraud Detection**: AI component accuracy testing

## CI/CD Pipeline

### Jenkins Pipeline

The project includes a comprehensive Jenkins pipeline (`Jenkinsfile`) that provides:

- **Multi-compiler builds** (GCC and Clang)
- **Static analysis** with clang-tidy and cppcheck
- **Code formatting** checks
- **Unit testing** with coverage analysis
- **Performance testing** and benchmarking
- **Security scanning** with Trivy
- **Docker containerization** and testing
- **Staging and production deployments**

#### Jenkins Setup

1. **Run the automated setup script**:
   ```bash
   # On your Jenkins agent, run the setup script
   ./jenkins-setup.sh

   # Or just verify installation without installing
   ./jenkins-setup.sh --verify-only

   # Or just create job configuration
   ./jenkins-setup.sh --create-job-config
   ```

2. **Install Jenkins** with required plugins:
   - Pipeline
   - Docker Pipeline
   - Warnings Next Generation
   - Coverage Plugin
   - JUnit Plugin

3. **Import Jenkins Job Configuration**:
   ```bash
   # The setup script creates jenkins-job-config.xml
   # Import this file into Jenkins to create the pipeline job
   ```

4. **Configure Automated Builds**:
   - Set up SCM polling or webhooks for automated builds
   - Configure credentials for Docker registry if needed

#### Pipeline Stages

1. **Checkout & Setup**: Source code checkout and build environment setup
2. **Static Analysis**: Code quality checks with clang-tidy and cppcheck
3. **Code Formatting**: Automated style checking
4. **Build**: Parallel compilation with GCC and Clang
5. **Unit Tests**: Test execution with coverage reporting
6. **Performance Tests**: Load testing and benchmarking
7. **Security Scan**: Vulnerability scanning with Trivy
8. **Docker Build**: Container image creation and testing
9. **Deployment**: Automated deployment to staging/production

#### Quality Gates

- **Static Analysis**: Code quality checks with clang-tidy
- **Security Scanning**: Vulnerability detection with Trivy
- **Performance Regression**: Automated benchmarking and load testing
- **Coverage Reports**: Code coverage analysis with lcov
- **Code Formatting**: Automated style checking with clang-format

## Security Features

### Authentication & Authorization

- **Session Management**: JWT-based authentication
- **Access Control**: Operation-level permissions
- **Audit Logging**: Comprehensive security events

### Network Security

- **TLS Support**: Encrypted communication channels
- **Message Integrity**: Cryptographic message verification
- **Rate Limiting**: DDoS protection mechanisms

## Monitoring and Observability

### Metrics Collection

- **Transaction Metrics**: Throughput, latency, error rates
- **System Metrics**: CPU, memory, network utilization
- **Business Metrics**: Account activity, fraud alerts

### Logging

- **Structured Logging**: JSON-formatted log entries
- **Correlation IDs**: Request tracing across components
- **Log Aggregation**: Centralized log management

### Alerting

- **Performance Alerts**: Latency and throughput thresholds
- **Security Alerts**: Fraud detection and suspicious activity
- **System Alerts**: Resource utilization and health checks

## Future Enhancements

### Planned Features

- **Inter-Bank Transfers**: Cross-branch transaction routing
- **Persistent Storage**: Database integration for durability
- **Advanced AI**: Deep learning fraud detection models
- **Microservices**: Component decomposition and containerization
- **Global Distribution**: Multi-region deployment support

### Research Areas

- **Blockchain Integration**: Distributed ledger technology
- **Quantum-Safe Crypto**: Post-quantum cryptographic algorithms
- **Edge Computing**: Client-side transaction processing
- **Real-Time Analytics**: Streaming data processing pipelines

## Contributing

### Development Setup

1. Fork the repository
2. Create a feature branch
3. Make changes with tests
4. Submit pull request

### Coding Standards

- **C++17**: Modern C++ features and idioms
- **RAII**: Resource management patterns
- **Exception Safety**: Comprehensive error handling
- **Documentation**: Doxygen-compatible comments

## License

MIT License - see LICENSE file for details.

## Acknowledgments

- Original banking system implementation
- Lock-free queue algorithms and concurrent programming patterns
- Fraud detection research and machine learning techniques
- High-performance networking and systems programming best practices 