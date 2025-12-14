-- Banking System Database Schema
-- PostgreSQL 12+ compatible

-- Enable UUID extension for transaction IDs
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- =============================================
-- CORE BANKING TABLES
-- =============================================

-- Accounts table
CREATE TABLE IF NOT EXISTS accounts (
    account_id VARCHAR(50) PRIMARY KEY,
    balance BIGINT NOT NULL DEFAULT 0 CHECK (balance >= 0),
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    is_active BOOLEAN NOT NULL DEFAULT TRUE
);

-- Transaction types enum
CREATE TYPE transaction_type AS ENUM (
    'DEPOSIT',
    'WITHDRAWAL',
    'TRANSFER_SEND',
    'TRANSFER_RECEIVE',
    'PAYMENT_SCHEDULED',
    'PAYMENT_PROCESSED',
    'ACCOUNT_MERGE',
    'ACCOUNT_CREATION'
);

-- Transactions table (immutable audit trail)
CREATE TABLE IF NOT EXISTS transactions (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    account_id VARCHAR(50) NOT NULL REFERENCES accounts(account_id),
    transaction_type transaction_type NOT NULL,
    amount BIGINT NOT NULL,
    balance_before BIGINT NOT NULL,
    balance_after BIGINT NOT NULL,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    reference_id VARCHAR(100), -- For related transactions (transfers, etc.)
    description TEXT,
    metadata JSONB -- Additional transaction data
);

-- =============================================
-- SCHEDULED PAYMENTS
-- =============================================

-- Scheduled payments table
CREATE TABLE IF NOT EXISTS scheduled_payments (
    payment_id VARCHAR(50) PRIMARY KEY,
    account_id VARCHAR(50) NOT NULL REFERENCES accounts(account_id),
    amount BIGINT NOT NULL CHECK (amount > 0),
    due_timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
    is_canceled BOOLEAN NOT NULL DEFAULT FALSE,
    is_processed BOOLEAN NOT NULL DEFAULT FALSE,
    processing_timestamp TIMESTAMP WITH TIME ZONE,
    creation_order SERIAL NOT NULL
);

-- =============================================
-- ACCOUNT RELATIONSHIPS
-- =============================================

-- Account merges (for historical queries)
CREATE TABLE IF NOT EXISTS account_merges (
    id SERIAL PRIMARY KEY,
    child_account_id VARCHAR(50) NOT NULL,
    parent_account_id VARCHAR(50) NOT NULL,
    merge_timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
    balance_transferred BIGINT NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,

    FOREIGN KEY (child_account_id) REFERENCES accounts(account_id),
    FOREIGN KEY (parent_account_id) REFERENCES accounts(account_id),

    UNIQUE(child_account_id, merge_timestamp)
);

-- =============================================
-- HISTORICAL BALANCE TRACKING
-- =============================================

-- Balance events for time-travel queries
CREATE TABLE IF NOT EXISTS balance_events (
    id SERIAL PRIMARY KEY,
    account_id VARCHAR(50) NOT NULL REFERENCES accounts(account_id),
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL,
    balance_delta BIGINT NOT NULL,
    event_type VARCHAR(50) NOT NULL, -- 'TRANSACTION', 'CREATION', 'MERGE', etc.

    UNIQUE(account_id, timestamp, event_type)
);

-- =============================================
-- AUDIT AND MONITORING
-- =============================================

-- System events (for observability)
CREATE TABLE IF NOT EXISTS system_events (
    id SERIAL PRIMARY KEY,
    event_type VARCHAR(100) NOT NULL,
    severity VARCHAR(20) NOT NULL DEFAULT 'INFO',
    message TEXT NOT NULL,
    component VARCHAR(100),
    correlation_id VARCHAR(100),
    metadata JSONB,
    timestamp TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- Fraud alerts (integrated with AI agent)
CREATE TABLE IF NOT EXISTS fraud_alerts (
    id SERIAL PRIMARY KEY,
    account_id VARCHAR(50) REFERENCES accounts(account_id),
    transaction_id UUID REFERENCES transactions(id),
    risk_score DECIMAL(3,2) NOT NULL CHECK (risk_score >= 0 AND risk_score <= 1),
    risk_factors TEXT[] NOT NULL DEFAULT '{}',
    recommendation VARCHAR(20) NOT NULL DEFAULT 'REVIEW',
    confidence_level INTEGER NOT NULL CHECK (confidence_level >= 0 AND confidence_level <= 100),
    is_resolved BOOLEAN NOT NULL DEFAULT FALSE,
    resolved_at TIMESTAMP WITH TIME ZONE,
    resolution_notes TEXT,
    created_at TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP
);

-- =============================================
-- INDEXES FOR PERFORMANCE
-- =============================================

-- Core banking indexes
CREATE INDEX IF NOT EXISTS idx_transactions_account_id ON transactions(account_id);
CREATE INDEX IF NOT EXISTS idx_transactions_timestamp ON transactions(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_transactions_type ON transactions(transaction_type);
CREATE INDEX IF NOT EXISTS idx_accounts_active ON accounts(is_active) WHERE is_active = TRUE;

-- Scheduled payments indexes
CREATE INDEX IF NOT EXISTS idx_scheduled_payments_account_id ON scheduled_payments(account_id);
CREATE INDEX IF NOT EXISTS idx_scheduled_payments_due ON scheduled_payments(due_timestamp) WHERE NOT is_processed AND NOT is_canceled;
CREATE INDEX IF NOT EXISTS idx_scheduled_payments_creation_order ON scheduled_payments(creation_order);

-- Historical data indexes
CREATE INDEX IF NOT EXISTS idx_balance_events_account_timestamp ON balance_events(account_id, timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_account_merges_child ON account_merges(child_account_id);

-- Audit and monitoring indexes
CREATE INDEX IF NOT EXISTS idx_system_events_timestamp ON system_events(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_system_events_correlation ON system_events(correlation_id);
CREATE INDEX IF NOT EXISTS idx_fraud_alerts_account ON fraud_alerts(account_id);
CREATE INDEX IF NOT EXISTS idx_fraud_alerts_risk ON fraud_alerts(risk_score DESC) WHERE NOT is_resolved;

-- =============================================
-- FUNCTIONS AND TRIGGERS
-- =============================================

-- Function to update account updated_at timestamp
CREATE OR REPLACE FUNCTION update_account_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Trigger for account updates
CREATE TRIGGER trigger_accounts_updated_at
    BEFORE UPDATE ON accounts
    FOR EACH ROW
    EXECUTE FUNCTION update_account_updated_at();

-- Function to validate transaction amounts
CREATE OR REPLACE FUNCTION validate_transaction_amount()
RETURNS TRIGGER AS $$
BEGIN
    -- Ensure balance doesn't go negative (except for allowed overdrafts)
    IF NEW.transaction_type IN ('WITHDRAWAL', 'TRANSFER_SEND', 'PAYMENT_PROCESSED') THEN
        -- Check if account has sufficient funds
        IF NEW.balance_before < NEW.amount THEN
            RAISE EXCEPTION 'Insufficient funds for account %: has %, needs %',
                NEW.account_id, NEW.balance_before, NEW.amount;
        END IF;
    END IF;

    -- Validate amounts are positive for relevant transaction types
    IF NEW.transaction_type IN ('DEPOSIT', 'WITHDRAWAL', 'TRANSFER_SEND', 'TRANSFER_RECEIVE', 'PAYMENT_SCHEDULED', 'PAYMENT_PROCESSED')
       AND NEW.amount <= 0 THEN
        RAISE EXCEPTION 'Invalid amount % for transaction type %', NEW.amount, NEW.transaction_type;
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- Trigger for transaction validation
CREATE TRIGGER trigger_validate_transaction
    BEFORE INSERT ON transactions
    FOR EACH ROW
    EXECUTE FUNCTION validate_transaction_amount();

-- Function to automatically process due payments
CREATE OR REPLACE FUNCTION process_due_payments()
RETURNS INTEGER AS $$
DECLARE
    payment_record RECORD;
    processed_count INTEGER := 0;
BEGIN
    -- Process all due payments that aren't canceled
    FOR payment_record IN
        SELECT * FROM scheduled_payments
        WHERE due_timestamp <= CURRENT_TIMESTAMP
        AND NOT is_canceled
        AND NOT is_processed
        ORDER BY creation_order
    LOOP
        -- Check account balance
        IF (SELECT balance FROM accounts WHERE account_id = payment_record.account_id) >= payment_record.amount THEN
            -- Deduct from account
            UPDATE accounts SET balance = balance - payment_record.amount WHERE account_id = payment_record.account_id;

            -- Record transaction
            INSERT INTO transactions (account_id, transaction_type, amount, balance_before, balance_after, reference_id)
            SELECT
                payment_record.account_id,
                'PAYMENT_PROCESSED'::transaction_type,
                payment_record.amount,
                balance + payment_record.amount, -- balance before deduction
                balance, -- balance after deduction
                payment_record.payment_id
            FROM accounts WHERE account_id = payment_record.account_id;

            -- Mark payment as processed
            UPDATE scheduled_payments
            SET is_processed = TRUE, processing_timestamp = CURRENT_TIMESTAMP
            WHERE payment_id = payment_record.payment_id;

            processed_count := processed_count + 1;
        END IF;
    END LOOP;

    RETURN processed_count;
END;
$$ LANGUAGE plpgsql;

-- =============================================
-- VIEWS FOR ANALYTICS
-- =============================================

-- Account summary view
CREATE OR REPLACE VIEW account_summary AS
SELECT
    a.account_id,
    a.balance,
    a.created_at,
    a.is_active,
    COUNT(t.id) as total_transactions,
    COALESCE(SUM(CASE WHEN t.transaction_type IN ('WITHDRAWAL', 'TRANSFER_SEND', 'PAYMENT_PROCESSED') THEN t.amount ELSE 0 END), 0) as total_outgoing,
    MAX(t.timestamp) as last_transaction
FROM accounts a
LEFT JOIN transactions t ON a.account_id = t.account_id
GROUP BY a.account_id, a.balance, a.created_at, a.is_active;

-- Top spenders view (matches the original API)
CREATE OR REPLACE VIEW top_spenders AS
SELECT
    account_id,
    total_outgoing as outgoing_amount
FROM account_summary
WHERE is_active = TRUE
ORDER BY total_outgoing DESC, account_id ASC;

-- =============================================
-- INITIAL DATA
-- =============================================

-- Insert initial system event
INSERT INTO system_events (event_type, severity, message, component)
VALUES ('SYSTEM_STARTUP', 'INFO', 'Database schema initialized', 'database')
ON CONFLICT DO NOTHING;
