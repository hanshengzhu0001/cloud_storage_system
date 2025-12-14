pipeline {
    agent any

    environment {
        BUILD_TYPE = 'Release'
        DOCKER_IMAGE = 'banking-system'
    }

    stages {
        stage('Checkout') {
            steps {
                echo 'Checking out source code...'
                checkout scm
            }
        }

        stage('Setup Build Environment') {
            steps {
                script {
                    if (isUnix()) {
                        sh '''
                            echo "Setting up Linux build environment..."
                            # Install system dependencies
                            apt-get update
                            apt-get install -y cmake ninja-build \
                                libgtest-dev libgmock-dev nlohmann-json3-dev \
                                g++ gcc lcov clang-format clang-tidy \
                                docker.io

                            # Create build directory
                            mkdir -p build
                        '''
                    } else {
                        bat '''
                            echo "Setting up Windows build environment..."
                            if not exist build mkdir build
                        '''
                    }
                }
            }
        }

        stage('Static Analysis') {
            steps {
                script {
                    if (isUnix()) {
                        sh '''
                            echo "Running clang-tidy..."
                            cd build
                            cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
                                -DCMAKE_CXX_CLANG_TIDY="clang-tidy;-checks=*"
                            make -j$(nproc) 2>&1 | tee clang-tidy-report.txt || true
                        '''
                    }
                }
            }
            post {
                always {
                    script {
                        if (fileExists('build/clang-tidy-report.txt')) {
                            archiveArtifacts artifacts: 'build/clang-tidy-report.txt',
                                           allowEmptyArchive: true
                        }
                    }
                }
            }
        }

        stage('Code Formatting Check') {
            steps {
                script {
                    if (isUnix()) {
                        sh '''
                            echo "Checking code formatting..."
                            find . -name "*.cpp" -o -name "*.hpp" | \
                            xargs clang-format --dry-run --Werror --style=file || true
                        '''
                    }
                }
            }
        }

        stage('Build') {
            parallel {
                stage('GCC Build') {
                    steps {
                        script {
                            if (isUnix()) {
                                sh '''
                                    echo "Building with GCC..."
                                    cd build
                                    rm -rf *
                                    CC=gcc CXX=g++ cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
                                        -DBUILD_TESTS=ON
                                    make -j$(nproc)
                                '''
                            }
                        }
                    }
                }

                stage('Clang Build') {
                    steps {
                        script {
                            if (isUnix()) {
                                sh '''
                                    echo "Building with Clang..."
                                    mkdir -p build_clang
                                    cd build_clang
                                    CC=clang CXX=clang++ cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
                                        -DBUILD_TESTS=ON
                                    make -j$(nproc)
                                '''
                            }
                        }
                    }
                }
            }
        }

        stage('Unit Tests') {
            steps {
                script {
                    if (isUnix()) {
                        sh '''
                            echo "Running unit tests..."
                            cd build
                            ctest --output-on-failure --parallel $(nproc) \
                                --output-junit test-results.xml
                        '''
                    }
                }
            }
            post {
                always {
                    junit 'build/test-results.xml'
                }
            }
        }

        stage('Coverage Analysis') {
            steps {
                script {
                    if (isUnix()) {
                        sh '''
                            echo "Generating code coverage..."
                            cd build
                            lcov --capture --directory . --output-file coverage.info \
                                --exclude "*/tests/*" --exclude "*/_deps/*" --exclude "*/usr/*"
                            lcov --list coverage.info
                        '''
                    }
                }
            }
            post {
                always {
                    script {
                        if (fileExists('build/coverage.info')) {
                            publishCoverage adapters: [
                                coberturaAdapter('build/coverage.info')
                            ]
                        }
                    }
                }
            }
        }

        stage('Performance Tests') {
            steps {
                script {
                    if (isUnix()) {
                        sh '''
                            echo "Running performance tests..."
                            cd build

                            # Start server in background
                            ./banking_server 8080 8 3600 &
                            SERVER_PID=$!
                            sleep 5

                            # Run load test (you would replace this with actual benchmark)
                            timeout 30 ./banking_client localhost 8080 || true

                            # Clean up
                            kill $SERVER_PID || true

                            # Generate performance report
                            echo "Performance test completed" > performance-report.txt
                        '''
                    }
                }
            }
            post {
                always {
                    archiveArtifacts artifacts: 'build/performance-report.txt',
                                   allowEmptyArchive: true
                }
            }
        }

        stage('Security Scan') {
            steps {
                script {
                    if (isUnix()) {
                        sh '''
                            echo "Running security scans..."

                            # Trivy container vulnerability scan
                            docker run --rm -v $(pwd):/root/.cache/trivy \
                                aquasecurity/trivy:0.40.0 image --exit-code 0 \
                                --no-progress --format json ubuntu:20.04 > trivy-results.json || true

                            # Static security analysis with cppcheck
                            cppcheck --enable=all --std=c++17 --language=c++ \
                                --xml --xml-version=2 . 2> cppcheck-results.xml || true
                        '''
                    }
                }
            }
            post {
                always {
                    archiveArtifacts artifacts: 'trivy-results.json,cppcheck-results.xml',
                                   allowEmptyArchive: true
                }
            }
        }

        stage('Docker Build') {
            steps {
                script {
                    sh '''
                        echo "Building Docker image..."
                        docker build -t ${DOCKER_IMAGE}:${BUILD_NUMBER} .
                        docker tag ${DOCKER_IMAGE}:${BUILD_NUMBER} ${DOCKER_IMAGE}:latest
                    '''
                }
            }
        }

        stage('Container Test') {
            steps {
                script {
                    sh '''
                        echo "Testing Docker container..."
                        # Run smoke test
                        docker run --rm -d --name banking-test-${BUILD_NUMBER} \
                            -p 8080:8080 ${DOCKER_IMAGE}:${BUILD_NUMBER} \
                            ./banking_server 8080 4 3600
                        sleep 5

                        # Test connectivity
                        docker run --rm ${DOCKER_IMAGE}:${BUILD_NUMBER} \
                            ./banking_client localhost 8080 || true

                        # Clean up
                        docker stop banking-test-${BUILD_NUMBER} || true
                        docker rm banking-test-${BUILD_NUMBER} || true
                    '''
                }
            }
        }

        stage('Deploy to Staging') {
            when {
                branch 'develop'
            }
            steps {
                script {
                    sh '''
                        echo "Deploying to staging environment..."
                        # Add your deployment commands here
                        # Example: deploy to Kubernetes, Docker Compose, etc.

                        # Tag for staging
                        docker tag ${DOCKER_IMAGE}:${BUILD_NUMBER} \
                            ${DOCKER_IMAGE}:staging-${BUILD_NUMBER}
                    '''
                }
            }
        }

        stage('Deploy to Production') {
            when {
                branch 'main'
                beforeInput true
            }
            input {
                message 'Deploy to production?'
                ok 'Deploy'
                submitterParameter 'APPROVER'
            }
            steps {
                script {
                    sh '''
                        echo "Deploying to production environment..."
                        # Add your production deployment commands here

                        # Tag for production
                        docker tag ${DOCKER_IMAGE}:${BUILD_NUMBER} \
                            ${DOCKER_IMAGE}:prod-${BUILD_NUMBER}

                        # Example deployment commands:
                        # kubectl apply -f k8s/production/
                        # docker-compose -f docker-compose.prod.yml up -d
                    '''
                }
            }
        }
    }

    post {
        always {
            script {
                sh '''
                    echo "Cleaning up build artifacts..."
                    # Clean up Docker images (keep latest few)
                    docker system prune -f || true
                '''
            }

            // Archive build artifacts
            archiveArtifacts artifacts: 'build/*.tar.gz,build/*.log',
                           allowEmptyArchive: true,
                           fingerprint: true
        }

        success {
            script {
                sh '''
                    echo "Pipeline completed successfully!"
                    echo "Build artifacts available for deployment."
                '''
            }
        }

        failure {
            script {
                sh '''
                    echo "Pipeline failed. Check logs for details."
                    # Send notifications here (email, Slack, etc.)
                '''
            }
        }

        unstable {
            script {
                sh '''
                    echo "Pipeline completed with warnings."
                    # Handle unstable builds (test failures, etc.)
                '''
            }
        }
    }

    options {
        timeout(time: 2, unit: 'HOURS')
        buildDiscarder(logRotator(numToKeepStr: '10'))
        disableConcurrentBuilds()
    }

    triggers {
        // Poll SCM every 5 minutes for changes
        pollSCM('H/5 * * * *')
    }
}
