#!/bin/bash
# Jenkins Setup Script for Banking System CI/CD
# This script helps configure a Jenkins environment for the banking system

set -e

echo "=== Banking System Jenkins Setup ==="
echo ""

# Check if running on supported OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "✓ Linux detected"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    echo "✓ macOS detected"
else
    echo "✗ Unsupported OS: $OSTYPE"
    exit 1
fi

# Function to install packages on Ubuntu/Debian
install_ubuntu_deps() {
    echo "Installing dependencies for Ubuntu/Debian..."

    sudo apt-get update

    # Install build tools
    sudo apt-get install -y cmake ninja-build

    # Install compilers
    sudo apt-get install -y g++ gcc clang clang-tidy clang-format

    # Install testing and coverage tools
    sudo apt-get install -y libgtest-dev libgmock-dev lcov

    # Install static analysis tools
    sudo apt-get install -y cppcheck

    # Install JSON library
    sudo apt-get install -y nlohmann-json3-dev

    # Install Docker
    sudo apt-get install -y docker.io

    # Configure Docker (optional - requires sudo)
    sudo usermod -aG docker $USER || true

    echo "✓ Ubuntu/Debian dependencies installed"
    echo "! You may need to log out and back in for Docker group changes to take effect"
}

# Function to install packages on macOS
install_macos_deps() {
    echo "Installing dependencies for macOS..."

    # Check if Homebrew is installed
    if ! command -v brew &> /dev/null; then
        echo "Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi

    # Install build tools
    brew install cmake ninja

    # Install compilers
    brew install gcc clang-format

    # Install testing and coverage tools
    brew install googletest lcov

    # Install JSON library
    brew install nlohmann-json

    # Install Docker (Docker Desktop for Mac)
    echo "Please install Docker Desktop for Mac manually from: https://www.docker.com/products/docker-desktop"

    echo "✓ macOS dependencies installed"
}

# Function to verify installation
verify_installation() {
    echo "Verifying installation..."

    local errors=0

    # Check build tools
    command -v cmake &> /dev/null || { echo "✗ cmake not found"; ((errors++)); }
    command -v ninja &> /dev/null || { echo "✗ ninja not found"; ((errors++)); }

    # Check compilers
    command -v g++ &> /dev/null || { echo "✗ g++ not found"; ((errors++)); }
    command -v clang &> /dev/null || { echo "✗ clang not found"; ((errors++)); }

    # Check analysis tools
    command -v clang-tidy &> /dev/null || { echo "✗ clang-tidy not found"; ((errors++)); }
    command -v clang-format &> /dev/null || { echo "✗ clang-format not found"; ((errors++)); }
    command -v cppcheck &> /dev/null || { echo "✗ cppcheck not found"; ((errors++)); }

    # Check Docker
    command -v docker &> /dev/null || { echo "! docker not found (optional for local builds)"; }

    if [ $errors -eq 0 ]; then
        echo "✓ All required tools verified"
        return 0
    else
        echo "✗ $errors tools missing"
        return 1
    fi
}

# Function to create Jenkins job configuration
create_jenkins_job_config() {
    echo "Creating Jenkins job configuration..."

    cat > jenkins-job-config.xml << 'EOF'
<?xml version='1.1' encoding='UTF-8'?>
<flow-definition plugin="workflow-job@2.40">
  <description>Banking System CI/CD Pipeline</description>
  <keepDependencies>false</keepDependencies>
  <properties>
    <org.jenkinsci.plugins.workflow.job.properties.DurabilityHintJobProperty plugin="workflow-durable-task-step@1121.v8b_4c6a_dc2a_38"/>
    <org.jenkinsci.plugins.workflow.job.properties.PipelineTriggersJobProperty>
      <triggers>
        <hudson.triggers.SCMTrigger>
          <spec>H/5 * * * *</spec>
          <ignorePostCommitHooks>false</ignorePostCommitHooks>
        </hudson.triggers.SCMTrigger>
      </triggers>
    </org.jenkinsci.plugins.workflow.job.properties.PipelineTriggersJobProperty>
  </properties>
  <definition class="org.jenkinsci.plugins.workflow.cps.CpsScmFlowDefinition" plugin="workflow-cps@2648.va9433432b33c">
    <scm class="hudson.plugins.git.GitSCM" plugin="git@4.11.3">
      <configVersion>2</configVersion>
      <userRemoteConfigs>
        <hudson.plugins.git.UserRemoteConfig>
          <url>.</url>
        </hudson.plugins.git.UserRemoteConfig>
      </userRemoteConfigs>
      <branches>
        <hudson.plugins.git.BranchSpec>
          <name>*/main</name>
        </hudson.plugins.git.BranchSpec>
        <hudson.plugins.git.BranchSpec>
          <name>*/develop</name>
        </hudson.plugins.git.BranchSpec>
      </branches>
      <doGenerateSubmoduleConfigurations>false</doGenerateSubmoduleConfigurations>
      <submoduleCfg class="list"/>
      <extensions/>
    </scm>
    <scriptPath>Jenkinsfile</scriptPath>
    <lightweight>true</lightweight>
  </definition>
  <triggers/>
  <quietPeriod>0</quietPeriod>
  <authToken>banking-ci-token</authToken>
</flow-definition>
EOF

    echo "✓ Jenkins job configuration created: jenkins-job-config.xml"
    echo "  Import this file into Jenkins to create the pipeline job"
}

# Function to display usage information
show_usage() {
    cat << EOF
Banking System Jenkins Setup Script

USAGE:
    $0 [OPTIONS]

OPTIONS:
    -h, --help          Show this help message
    --verify-only       Only verify installation, don't install anything
    --create-job-config Only create Jenkins job configuration

DESCRIPTION:
    This script sets up the necessary tools and dependencies for running
    the Banking System CI/CD pipeline on Jenkins.

    It will:
    1. Detect your operating system
    2. Install required build tools and dependencies
    3. Verify the installation
    4. Create Jenkins job configuration file

    Required tools:
    - cmake, ninja (build system)
    - gcc, clang (compilers)
    - clang-tidy, clang-format (code analysis)
    - cppcheck (static analysis)
    - gtest, gmock (unit testing)
    - lcov (code coverage)
    - docker (containerization)

JENKINS SETUP:
    1. Install Jenkins and required plugins
    2. Run this setup script on your Jenkins agent
    3. Import the generated jenkins-job-config.xml
    4. Configure SCM webhook or polling for automated builds

EOF
}

# Main script logic
main() {
    local verify_only=false
    local create_job_only=false

    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_usage
                exit 0
                ;;
            --verify-only)
                verify_only=true
                shift
                ;;
            --create-job-config)
                create_job_only=true
                shift
                ;;
            *)
                echo "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done

    if $create_job_only; then
        create_jenkins_job_config
        exit 0
    fi

    if $verify_only; then
        verify_installation
        exit $?
    fi

    # Detect OS and install dependencies
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if command -v apt-get &> /dev/null; then
            install_ubuntu_deps
        else
            echo "✗ Unsupported Linux distribution (only Ubuntu/Debian supported)"
            exit 1
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        install_macos_deps
    else
        echo "✗ Unsupported OS: $OSTYPE"
        exit 1
    fi

    # Verify installation
    if verify_installation; then
        echo ""
        create_jenkins_job_config
        echo ""
        echo "=== Setup Complete ==="
        echo "You can now:"
        echo "1. Import jenkins-job-config.xml into Jenkins"
        echo "2. Configure webhooks or polling for automated builds"
        echo "3. Run your first build!"
        echo ""
        echo "For local development:"
        echo "  mkdir build && cd build"
        echo "  cmake .. && make -j$(nproc)"
        echo "  ./banking_server 8080 4 3600"
        echo "  # In another terminal: ./banking_client localhost 8080"
    else
        echo "✗ Setup failed. Please check the errors above."
        exit 1
    fi
}

# Run main function
main "$@"
