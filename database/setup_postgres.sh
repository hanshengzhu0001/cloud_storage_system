#!/bin/bash
# PostgreSQL Setup Script for Banking System
# This script sets up PostgreSQL database and user for the banking system

set -e

# Default configuration
DB_HOST="${DB_HOST:-localhost}"
DB_PORT="${DB_PORT:-5432}"
DB_NAME="${DB_NAME:-banking_system}"
DB_USERNAME="${DB_USERNAME:-banking_user}"
DB_PASSWORD="${DB_PASSWORD:-secure_password_123}"
POSTGRES_USER="${POSTGRES_USER:-postgres}"

echo "=== Banking System PostgreSQL Setup ==="
echo "Database Host: $DB_HOST"
echo "Database Port: $DB_PORT"
echo "Database Name: $DB_NAME"
echo "Database User: $DB_USERNAME"
echo ""

# Function to check if PostgreSQL is running
check_postgres() {
    if ! pg_isready -h "$DB_HOST" -p "$DB_PORT" >/dev/null 2>&1; then
        echo "❌ PostgreSQL is not running on $DB_HOST:$DB_PORT"
        echo "Please start PostgreSQL service:"
        echo "  Ubuntu/Debian: sudo systemctl start postgresql"
        echo "  macOS: brew services start postgresql"
        echo "  Docker: docker run -d --name postgres -p $DB_PORT:5432 -e POSTGRES_PASSWORD=postgres postgres:13"
        exit 1
    fi
    echo "✅ PostgreSQL is running"
}

# Function to check if we're running as superuser
check_superuser() {
    if ! psql -h "$DB_HOST" -p "$DB_PORT" -U "$POSTGRES_USER" -c "SELECT 1;" >/dev/null 2>&1; then
        echo "❌ Cannot connect to PostgreSQL as superuser ($POSTGRES_USER)"
        echo "Please ensure PostgreSQL is running and you have superuser access"
        exit 1
    fi
    echo "✅ Superuser access confirmed"
}

# Function to create database and user
setup_database() {
    echo "Creating database and user..."

    # Create user if it doesn't exist
    psql -h "$DB_HOST" -p "$DB_PORT" -U "$POSTGRES_USER" -c "
        DO \$\$
        BEGIN
            IF NOT EXISTS (SELECT FROM pg_catalog.pg_roles WHERE rolname = '$DB_USERNAME') THEN
                CREATE ROLE $DB_USERNAME LOGIN PASSWORD '$DB_PASSWORD';
            END IF;
        END
        \$\$;
    " >/dev/null

    # Create database if it doesn't exist
    psql -h "$DB_HOST" -p "$DB_PORT" -U "$POSTGRES_USER" -c "
        SELECT 'CREATE DATABASE $DB_NAME OWNER $DB_USERNAME'
        WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = '$DB_NAME')\gexec
    " >/dev/null

    # Grant permissions
    psql -h "$DB_HOST" -p "$DB_PORT" -U "$POSTGRES_USER" -c "
        GRANT ALL PRIVILEGES ON DATABASE $DB_NAME TO $DB_USERNAME;
        ALTER USER $DB_USERNAME CREATEDB;
    " >/dev/null

    echo "✅ Database and user created successfully"
}

# Function to initialize schema
initialize_schema() {
    echo "Initializing database schema..."

    if [ ! -f "database/schema.sql" ]; then
        echo "❌ Schema file not found: database/schema.sql"
        echo "Please run this script from the banking_system root directory"
        exit 1
    fi

    # Connect to the database and run schema
    psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USERNAME" -d "$DB_NAME" \
         -f "database/schema.sql" >/dev/null

    echo "✅ Database schema initialized"
}

# Function to test connection
test_connection() {
    echo "Testing database connection..."

    # Test basic connection
    if ! psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USERNAME" -d "$DB_NAME" \
         -c "SELECT version();" >/dev/null 2>&1; then
        echo "❌ Cannot connect to database with user $DB_USERNAME"
        exit 1
    fi

    # Test schema creation
    if ! psql -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USERNAME" -d "$DB_NAME" \
         -c "SELECT COUNT(*) FROM accounts;" >/dev/null 2>&1; then
        echo "❌ Database schema not properly initialized"
        exit 1
    fi

    echo "✅ Database connection and schema verified"
}

# Function to show connection details
show_connection_info() {
    echo ""
    echo "=== Database Connection Information ==="
    echo "Host: $DB_HOST"
    echo "Port: $DB_PORT"
    echo "Database: $DB_NAME"
    echo "Username: $DB_USERNAME"
    echo "Password: $DB_PASSWORD"
    echo ""
    echo "=== Connection String ==="
    echo "postgresql://$DB_USERNAME:$DB_PASSWORD@$DB_HOST:$DB_PORT/$DB_NAME"
    echo ""
    echo "=== Server Startup Command ==="
    echo "./banking_server 8080 4 3600 $DB_HOST $DB_PORT $DB_NAME $DB_USERNAME $DB_PASSWORD"
}

# Main execution
main() {
    echo "Setting up PostgreSQL database for Banking System..."
    echo ""

    check_postgres
    check_superuser
    setup_database
    initialize_schema
    test_connection

    echo ""
    echo "🎉 PostgreSQL setup completed successfully!"
    echo ""

    show_connection_info

    echo "=== Next Steps ==="
    echo "1. Start the banking server with database support:"
    echo "   ./banking_server 8080 4 3600 $DB_HOST $DB_PORT $DB_NAME $DB_USERNAME $DB_PASSWORD"
    echo ""
    echo "2. Test the connection with the client:"
    echo "   ./banking_client localhost 8080"
    echo ""
    echo "3. Check logs for database operations and performance metrics"
}

# Environment variable override notice
if [ -n "$DB_PASSWORD" ] && [ "$DB_PASSWORD" != "secure_password_123" ]; then
    echo "ℹ️  Using custom database password from environment"
fi

# Run main function
main "$@"
